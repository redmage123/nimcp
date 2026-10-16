/**
 * @file nimcp_gpu_execution_immune_bridge.c
 * @brief GPU Execution Mode-Immune System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-12
 */

#include "gpu/immune/nimcp_gpu_execution_immune_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(gpu_execution_immune_bridge)

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Get current time in milliseconds
 *
 * WHAT: Get system time for duration tracking
 * WHY:  Track execution mode timing
 * HOW:  Platform-specific time retrieval
 */
static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/**
 * @brief Get inflammation execution mode preference
 */
static execution_mode_t get_inflammation_mode_preference(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return INFLAMMATION_MODE_NONE_PREFERENCE;
        case INFLAMMATION_LOCAL:    return INFLAMMATION_MODE_LOCAL_PREFERENCE;
        case INFLAMMATION_REGIONAL: return INFLAMMATION_MODE_REGIONAL_PREFERENCE;
        case INFLAMMATION_SYSTEMIC: return INFLAMMATION_MODE_SYSTEMIC_PREFERENCE;
        case INFLAMMATION_STORM:    return INFLAMMATION_MODE_STORM_PREFERENCE;
        default:                    return EXEC_MODE_GPU_CUDA;
    }
}

/**
 * @brief Get energy conservation factor for mode
 */
static float get_mode_energy_factor(execution_mode_t mode) {
    switch (mode) {
        case EXEC_MODE_CPU_SEQUENTIAL:  return EXEC_ENERGY_FACTOR_CPU_SEQUENTIAL;
        case EXEC_MODE_CPU_PARALLEL:    return EXEC_ENERGY_FACTOR_CPU_PARALLEL;
        case EXEC_MODE_HYBRID:          return EXEC_ENERGY_FACTOR_HYBRID;
        case EXEC_MODE_GPU_CUDA:
        case EXEC_MODE_GPU_ROCM:
        case EXEC_MODE_GPU_OPENCL:      return EXEC_ENERGY_FACTOR_GPU;
        default:                        return 1.0f;
    }
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int execution_immune_default_config(execution_immune_config_t* config) {
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    /* Enable all features by default */
    config->enable_cytokine_mode_modulation = true;
    config->enable_exec_error_immune_response = true;
    config->enable_energy_conservation = true;
    config->enable_fallback_modulation = true;
    config->enable_performance_monitoring = true;

    /* Default sensitivity */
    config->cytokine_sensitivity = 1.0f;
    config->error_sensitivity = 1.0f;

    /* Mode selection */
    config->baseline_mode = EXEC_MODE_GPU_CUDA;
    config->allow_mode_switching = true;

    /* Thresholds */
    config->performance_degradation_threshold = 0.7f;
    config->error_window_ms = 5000;
    config->max_errors_per_window = 3;

    return NIMCP_SUCCESS;
}

execution_immune_bridge_t* execution_immune_create(
    const execution_immune_config_t* config,
    brain_immune_system_t* immune_system,
    execution_context_t exec_context
) {
    if (!immune_system) {
        NIMCP_LOGGING_ERROR("execution_immune_create: immune_system required");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_system is NULL");

        return NULL;
    }

    execution_immune_bridge_t* bridge = nimcp_malloc(sizeof(execution_immune_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("execution_immune_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    memset(bridge, 0, sizeof(execution_immune_bridge_t));

    execution_immune_config_t default_config;
    if (!config) {
        execution_immune_default_config(&default_config);
        config = &default_config;
    }

    bridge->enable_cytokine_mode_modulation = config->enable_cytokine_mode_modulation;
    bridge->enable_exec_error_immune_response = config->enable_exec_error_immune_response;
    bridge->enable_energy_conservation = config->enable_energy_conservation;
    bridge->enable_fallback_modulation = config->enable_fallback_modulation;
    bridge->enable_performance_monitoring = config->enable_performance_monitoring;

    bridge->immune_system = immune_system;
    bridge->exec_context = exec_context;
    bridge->baseline_mode = config->baseline_mode;
    bridge->allow_mode_switching = config->allow_mode_switching;
    bridge->cytokine_effects.preferred_mode = config->baseline_mode;

    bridge->last_update_time = get_time_ms();
    bridge->error_window_start = bridge->last_update_time;

    if (bridge_base_init(&bridge->base, 0, "gpu_execution_immune") != 0) { nimcp_free(bridge); return NULL; }

    NIMCP_LOGGING_INFO("execution_immune_bridge: created successfully");
    return bridge;
}

void execution_immune_destroy(execution_immune_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    if (bridge->base.bio_async_enabled) {
        execution_immune_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

/* ============================================================================
 * Immune → Execution API
 * ============================================================================ */

int execution_immune_apply_cytokine_effects(execution_immune_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->immune_system, NIMCP_ERROR_NULL_POINTER, "bridge->immune_system is NULL");

    if (!bridge->enable_cytokine_mode_modulation) {
        return NIMCP_SUCCESS;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    brain_immune_stats_t stats;
    brain_immune_get_stats(bridge->immune_system, &stats);

    memset(&bridge->cytokine_effects, 0, sizeof(execution_cytokine_effects_t));

    /* Use inflammation level from immune stats */
    brain_inflammation_level_t level = stats.inflammation_level;

    /* Set mode preference */
    bridge->cytokine_effects.preferred_mode = get_inflammation_mode_preference(level);
    bridge->cytokine_effects.energy_conservation_factor = get_mode_energy_factor(
        bridge->cytokine_effects.preferred_mode
    );

    /* Force CPU-only during storm */
    bridge->cytokine_effects.force_cpu_only = (level == INFLAMMATION_STORM);

    /* Enable aggressive fallback during systemic/storm */
    bridge->cytokine_effects.enable_aggressive_fallback =
        (level >= INFLAMMATION_SYSTEMIC);

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

execution_mode_t execution_immune_get_recommended_mode(
    const execution_immune_bridge_t* bridge
) {
    if (!bridge) {
        return EXEC_MODE_GPU_CUDA;
    }

    if (bridge->cytokine_effects.force_cpu_only) {
        return EXEC_MODE_CPU_PARALLEL;
    }

    return bridge->cytokine_effects.preferred_mode;
}

float execution_immune_get_energy_factor(const execution_immune_bridge_t* bridge) {
    if (!bridge) {
        return 1.0f;
    }

    return bridge->cytokine_effects.energy_conservation_factor;
}

/* ============================================================================
 * Execution → Immune API
 * ============================================================================ */

int execution_immune_trigger_error_response(
    execution_immune_bridge_t* bridge,
    int error_code,
    const char* error_message
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->immune_system, NIMCP_ERROR_NULL_POINTER, "bridge->immune_system is NULL");

    if (!bridge->enable_exec_error_immune_response) {
        return NIMCP_SUCCESS;
    }

    uint8_t severity = EXEC_ERROR_SEVERITY_INIT_FAILURE;

    const char* msg = error_message ? error_message : "Execution error";
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
        NIMCP_LOGGING_WARN("execution_immune: triggered immune response for exec error %d", error_code);
    }

    return result;
}

int execution_immune_monitor_performance(execution_immune_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->exec_context, NIMCP_ERROR_NULL_POINTER, "bridge->exec_context is NULL");

    if (!bridge->enable_performance_monitoring) {
        return NIMCP_SUCCESS;
    }

    /* Query execution stats */
    uint64_t total_ops;
    double total_time_ms;

    if (!execution_get_stats(bridge->exec_context, &total_ops, &total_time_ms)) {
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    /* Update performance metrics */
    if (total_time_ms > 0) {
        bridge->error_state.current_ops_per_sec = total_ops / (total_time_ms / 1000.0);
    }

    /* Check for degradation */
    if (bridge->error_state.baseline_ops_per_sec > 0) {
        bridge->error_state.performance_ratio =
            bridge->error_state.current_ops_per_sec / bridge->error_state.baseline_ops_per_sec;

        if (bridge->error_state.performance_ratio < 0.7f) {
            bridge->error_state.performance_degraded = true;

            /* Trigger immune response */
            const char* msg = "Execution performance degraded";
            uint32_t antigen_id;
            brain_immune_present_antigen(
                bridge->immune_system,
                ANTIGEN_SOURCE_MANUAL,
                (const uint8_t*)msg,
                strlen(msg),
                EXEC_ERROR_SEVERITY_VALIDATION_FAIL,
                0,
                &antigen_id
            );
        }
    }

    return NIMCP_SUCCESS;
}

int execution_immune_update_error_state(execution_immune_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->exec_context, NIMCP_ERROR_NULL_POINTER, "bridge->exec_context is NULL");

    bridge->error_state.active_mode = execution_context_get_mode(bridge->exec_context);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

int execution_immune_update(execution_immune_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    execution_immune_update_error_state(bridge);
    execution_immune_apply_cytokine_effects(bridge);
    execution_immune_monitor_performance(bridge);

    bridge->total_updates++;
    bridge->last_update_time = get_time_ms();

    return NIMCP_SUCCESS;
}

int execution_immune_apply_modulation(execution_immune_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->exec_context, NIMCP_ERROR_NULL_POINTER, "bridge->exec_context is NULL");

    if (!bridge->allow_mode_switching) {
        return NIMCP_SUCCESS;
    }

    execution_mode_t recommended = execution_immune_get_recommended_mode(bridge);
    execution_mode_t current = execution_context_get_mode(bridge->exec_context);

    if (recommended != current) {
        if (execution_context_set_mode(bridge->exec_context, recommended)) {
            bridge->mode_changes++;
            NIMCP_LOGGING_INFO("execution_immune: changed mode to %d", recommended);
        }
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int execution_immune_get_cytokine_effects(
    const execution_immune_bridge_t* bridge,
    execution_cytokine_effects_t* effects
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(effects, NIMCP_ERROR_NULL_POINTER, "effects is NULL");

    memcpy(effects, &bridge->cytokine_effects, sizeof(execution_cytokine_effects_t));
    return NIMCP_SUCCESS;
}

int execution_immune_get_error_state(
    const execution_immune_bridge_t* bridge,
    execution_error_state_t* state
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_NULL_POINTER, "state is NULL");

    memcpy(state, &bridge->error_state, sizeof(execution_error_state_t));
    return NIMCP_SUCCESS;
}

bool execution_immune_is_mode_changed(const execution_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    return bridge->cytokine_effects.preferred_mode != bridge->baseline_mode;
}

float execution_immune_get_energy_conservation_factor(
    const execution_immune_bridge_t* bridge
) {
    return execution_immune_get_energy_factor(bridge);
}

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

int execution_immune_connect_bio_async(execution_immune_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_GPU_EXECUTION,
        .module_name = "execution_immune_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("execution_immune: connected to bio-async router");
        return NIMCP_SUCCESS;
    }

    NIMCP_LOGGING_WARN("execution_immune: bio-async router not available");
    return NIMCP_ERROR_OPERATION_FAILED;
}

int execution_immune_disconnect_bio_async(execution_immune_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS;
    }

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("execution_immune: disconnected from bio-async router");

    return NIMCP_SUCCESS;
}

bool execution_immune_is_bio_async_connected(const execution_immune_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}
