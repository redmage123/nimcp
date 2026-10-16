/**
 * @file nimcp_circular_buffer_fep_bridge.c
 * @brief Free Energy Principle - Circular Buffer Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-12
 */

#include "middleware/buffering/nimcp_circular_buffer_fep_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(circular_buffer_fep_bridge)

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * WHAT: Provide default configuration
 * WHY:  Sensible defaults for easy initialization
 * HOW:  Set biologically-plausible values
 */
int circular_buffer_fep_bridge_default_config(
    circular_buffer_fep_config_t* config
) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    config->enable_horizon_adjustment = true;
    config->enable_precision_windowing = true;
    config->enable_overflow_surprise = true;
    config->enable_utilization_feedback = true;

    config->horizon_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->precision_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->overflow_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->utilization_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    return 0;
}

/**
 * WHAT: Create buffer-FEP bridge
 * WHY:  Initialize integration
 * HOW:  Allocate, set defaults, create mutex
 */
circular_buffer_fep_bridge_t* circular_buffer_fep_bridge_create(
    const circular_buffer_fep_config_t* config
) {
    circular_buffer_fep_bridge_t* bridge = (circular_buffer_fep_bridge_t*)
        nimcp_malloc(sizeof(circular_buffer_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate buffer-FEP bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    memset(bridge, 0, sizeof(circular_buffer_fep_bridge_t));

    if (config) {
        bridge->config = *config;
    } else {
        circular_buffer_fep_bridge_default_config(&bridge->config);
    }

    if (bridge_base_init(&bridge->base, 0, "circular_buffer_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "circular_buffer_fep_bridge_create: bridge->base is NULL");
        return NULL;
    }

    bridge->effects.target_capacity = 64;
    bridge->effects.attention_window = FEP_BUFFER_PRECISION_WINDOW_BASE;
    bridge->effects.horizon_depth = 32;
    bridge->effects.overflow_tolerance = 0.1f;

    NIMCP_LOGGING_INFO("Created buffer-FEP bridge");
    return bridge;
}

/**
 * WHAT: Destroy bridge
 * WHY:  Clean resource cleanup
 * HOW:  Disconnect, destroy mutex, free memory
 */
void circular_buffer_fep_bridge_destroy(
    circular_buffer_fep_bridge_t* bridge
) {
    if (!bridge) return;

    circular_buffer_fep_bridge_disconnect(bridge);

    if (bridge->base.bio_async_enabled) {
        circular_buffer_fep_bridge_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed buffer-FEP bridge");
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * WHAT: Connect circular buffer
 * WHY:  Enable buffer access
 * HOW:  Store pointer
 */
int circular_buffer_fep_bridge_connect_buffer(
    circular_buffer_fep_bridge_t* bridge,
    circular_buffer_t* buffer
) {
    if (!bridge || !buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "circular_buffer_fep_bridge_connect_buffer: required parameter is NULL (bridge, buffer)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->buffer = buffer;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected circular buffer to FEP bridge");
    return 0;
}

/**
 * WHAT: Connect FEP system
 * WHY:  Enable FEP access
 * HOW:  Store pointer
 */
int circular_buffer_fep_bridge_connect_fep(
    circular_buffer_fep_bridge_t* bridge,
    fep_system_t* fep
) {
    if (!bridge || !fep) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "circular_buffer_fep_bridge_connect_fep: required parameter is NULL (bridge, fep)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected FEP system to buffer bridge");
    return 0;
}

/**
 * WHAT: Disconnect all systems
 * WHY:  Safe shutdown
 * HOW:  Clear pointers
 */
int circular_buffer_fep_bridge_disconnect(
    circular_buffer_fep_bridge_t* bridge
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->buffer = NULL;
    bridge->fep_system = NULL;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * FEP → Buffer Direction
 * ============================================================================ */

/**
 * WHAT: Adjust buffer capacity based on horizon
 * WHY:  Match buffer depth to prediction needs
 * HOW:  Scale capacity with horizon
 */
int circular_buffer_fep_adjust_horizon(
    circular_buffer_fep_bridge_t* bridge,
    uint32_t horizon
) {
    if (!bridge || !bridge->config.enable_horizon_adjustment) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "circular_buffer_fep_adjust_horizon: required parameter is NULL (bridge, bridge->config)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t target_capacity = (uint32_t)(horizon * bridge->config.horizon_sensitivity);
    if (target_capacity < FEP_BUFFER_MIN_HORIZON) {
        target_capacity = FEP_BUFFER_MIN_HORIZON;
    }
    if (target_capacity > FEP_BUFFER_MAX_HORIZON) {
        target_capacity = FEP_BUFFER_MAX_HORIZON;
    }

    bridge->effects.target_capacity = target_capacity;
    bridge->effects.horizon_depth = horizon;
    bridge->stats.horizon_adjustments++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Set attention window from precision
 * WHY:  Focus on recent vs. distant history
 * HOW:  Scale window with precision
 */
int circular_buffer_fep_set_precision_window(
    circular_buffer_fep_bridge_t* bridge,
    float precision
) {
    if (!bridge || !bridge->config.enable_precision_windowing) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "circular_buffer_fep_set_precision_window: required parameter is NULL (bridge, bridge->config)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t window = (uint32_t)(FEP_BUFFER_PRECISION_WINDOW_BASE +
                                  precision * 40.0f * bridge->config.precision_sensitivity);
    bridge->effects.attention_window = window;
    bridge->state.current_precision = precision;
    bridge->stats.window_adjustments++;
    bridge->stats.avg_attention_window =
        (bridge->stats.avg_attention_window * 0.9f) + (window * 0.1f);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Prime buffer for expected sequence
 * WHY:  Prepare for predicted pattern
 * HOW:  Set priming flag
 */
int circular_buffer_fep_prime_sequence(
    circular_buffer_fep_bridge_t* bridge,
    float expected_fill_rate
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->effects.primed_for_sequence = true;
    bridge->effects.overflow_tolerance = expected_fill_rate * 0.2f;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Buffer → FEP Direction
 * ============================================================================ */

/**
 * WHAT: Report utilization to FEP
 * WHY:  High utilization = capacity constraint
 * HOW:  Map to uncertainty
 */
int circular_buffer_fep_report_utilization(
    circular_buffer_fep_bridge_t* bridge,
    float utilization
) {
    if (!bridge || !bridge->config.enable_utilization_feedback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "circular_buffer_fep_report_utilization: required parameter is NULL (bridge, bridge->config)");
        return -1;
    }
    if (!bridge->buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "circular_buffer_fep_report_utilization: bridge->buffer is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->state.buffer_utilization = utilization;
    bridge->stats.avg_utilization =
        (bridge->stats.avg_utilization * 0.9f) + (utilization * 0.1f);

    if (utilization > FEP_BUFFER_HIGH_UTILIZATION) {
        bridge->state.capacity_constraint =
            (utilization - FEP_BUFFER_HIGH_UTILIZATION) *
            bridge->config.utilization_sensitivity;
    } else {
        bridge->state.capacity_constraint = 0.0f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Report overflow as surprise
 * WHY:  Overflow = failed prediction
 * HOW:  Generate surprise signal
 */
int circular_buffer_fep_report_overflow(
    circular_buffer_fep_bridge_t* bridge,
    uint32_t overflow_count
) {
    if (!bridge || !bridge->config.enable_overflow_surprise) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "circular_buffer_fep_report_overflow: required parameter is NULL (bridge, bridge->config)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->state.overflow_count = overflow_count;
    bridge->stats.total_overflows += overflow_count;

    if (overflow_count > 0) {
        float surprise = FEP_BUFFER_OVERFLOW_SURPRISE * overflow_count *
                        bridge->config.overflow_sensitivity;
        bridge->state.temporal_surprise = surprise;
        bridge->stats.surprise_events++;
        bridge->stats.avg_overflow_surprise =
            (bridge->stats.avg_overflow_surprise * 0.9f) + (surprise * 0.1f);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Report buffer patterns
 * WHY:  Temporal structure observation
 * HOW:  Extract and report patterns
 */
int circular_buffer_fep_report_patterns(
    circular_buffer_fep_bridge_t* bridge
) {
    if (!bridge || !bridge->buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "circular_buffer_fep_report_patterns: required parameter is NULL (bridge, bridge->buffer)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    size_t size = circular_buffer_size(bridge->buffer);
    size_t capacity = circular_buffer_capacity(bridge->buffer);

    bridge->state.buffer_size = (uint32_t)size;
    float utilization = (capacity > 0) ? ((float)size / capacity) : 0.0f;
    circular_buffer_fep_report_utilization(bridge, utilization);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

/**
 * WHAT: Main update loop
 * WHY:  Synchronize buffer and FEP
 * HOW:  Update horizon, window, check overflows
 */
int circular_buffer_fep_bridge_update(
    circular_buffer_fep_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->buffer || !bridge->fep_system) return 0;

    circular_buffer_fep_report_patterns(bridge);

    return 0;
}

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

/**
 * WHAT: Get current state
 * WHY:  State inspection
 * HOW:  Copy state struct
 */
int circular_buffer_fep_bridge_get_state(
    const circular_buffer_fep_bridge_t* bridge,
    circular_buffer_fep_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "circular_buffer_fep_bridge_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/**
 * WHAT: Get statistics
 * WHY:  Performance monitoring
 * HOW:  Copy stats struct
 */
int circular_buffer_fep_bridge_get_stats(
    const circular_buffer_fep_bridge_t* bridge,
    circular_buffer_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "circular_buffer_fep_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * WHAT: Connect to bio-async router
 * WHY:  Enable distributed signaling
 * HOW:  Register module
 */
int circular_buffer_fep_bridge_connect_bio_async(
    circular_buffer_fep_bridge_t* bridge
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_BUFFERING_BRIDGE,
        .module_name = "circular_buffer_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "circular_buffer_fep_bridge_connect_bio_async: validation failed");
    return -1;
}

/**
 * WHAT: Disconnect from bio-async
 * WHY:  Clean shutdown
 * HOW:  Unregister module
 */
int circular_buffer_fep_bridge_disconnect_bio_async(
    circular_buffer_fep_bridge_t* bridge
) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "circular_buffer_fep_bridge_disconnect_bio_async: required parameter is NULL (bridge, bridge->base)");
        return -1;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    return 0;
}

/**
 * WHAT: Check bio-async connection
 * WHY:  Query connection state
 * HOW:  Return enabled flag
 */
bool circular_buffer_fep_bridge_is_bio_async_connected(
    const circular_buffer_fep_bridge_t* bridge
) {
    return bridge ? bridge->base.bio_async_enabled : false;
}
