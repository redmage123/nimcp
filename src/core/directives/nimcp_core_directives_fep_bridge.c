/**
 * @file nimcp_core_directives_fep_bridge.c
 * @brief Core Directives FEP Bridge Implementation
 */

#include "core/directives/nimcp_core_directives_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define LOG_MODULE_DIRECTIVE_FEP "[DIRECTIVE_FEP]"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(core_directives_fep_bridge)

/* Exponential moving average decay for statistics */
#define EMA_ALPHA 0.1f

/* Free energy penalties */
#define FREE_ENERGY_BLOCKED_PENALTY   10.0f   /* High FE for blocked actions */
#define FREE_ENERGY_ALLOWED_BASE      0.5f    /* Low FE for allowed actions */

/* Surprise thresholds */
#define SURPRISE_BLOCKED_BASE         0.9f    /* High surprise when blocked */
#define SURPRISE_ALLOWED_BASE         0.1f    /* Low surprise when allowed */

/* Prediction error scaling */
#define PREDICTION_ERROR_SCALE        2.0f

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Populate config with sensible defaults
 * WHY:  Simplifies initialization
 * HOW:  Set constants for prediction error, surprise, precision
 */
int directive_fep_bridge_default_config(directive_fep_config_t* config) {
    if (!config) {
        NIMCP_FEP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    config->prediction_error_weight = DIRECTIVE_FEP_PREDICTION_ERROR_WEIGHT_DEFAULT;
    config->surprise_threshold = DIRECTIVE_FEP_SURPRISE_THRESHOLD_DEFAULT;
    config->precision_default = DIRECTIVE_FEP_PRECISION_DEFAULT;
    config->enable_active_inference = true;

    return 0;
}

/**
 * @brief Create directive FEP bridge
 *
 * WHAT: Allocate and initialize bridge
 * WHY:  Entry point for FEP-Directives integration
 * HOW:  Allocate struct, initialize mutex, set defaults
 */
directive_fep_bridge_t* directive_fep_bridge_create(
    const directive_fep_config_t* config,
    core_directives_t* core_directives,
    fep_system_t* fep_orchestrator
) {
    directive_fep_bridge_t* bridge = (directive_fep_bridge_t*)nimcp_malloc(
        sizeof(directive_fep_bridge_t)
    );
    if (!bridge) {
        NIMCP_LOGGING_ERROR(LOG_MODULE_DIRECTIVE_FEP " Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    memset(bridge, 0, sizeof(directive_fep_bridge_t));

    /* Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        directive_fep_bridge_default_config(&bridge->config);
    }

    /* Set connections */
    bridge->core_directives = core_directives;
    bridge->fep_orchestrator = fep_orchestrator;

    /* Initialize state */
    bridge->state.free_energy = 0.0f;
    bridge->state.prediction_error = 0.0f;
    bridge->state.surprise = 0.0f;
    bridge->state.precision = bridge->config.precision_default;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "core_directives_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR(LOG_MODULE_DIRECTIVE_FEP " Failed to create mutex");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "directive_fep_bridge_create: bridge->base is NULL");
        return NULL;
    }

    NIMCP_LOGGING_INFO(LOG_MODULE_DIRECTIVE_FEP " Bridge created");
    return bridge;
}

/**
 * @brief Destroy directive FEP bridge
 *
 * WHAT: Free all resources
 * WHY:  Cleanup on shutdown
 * HOW:  Disconnect bio-async, destroy mutex, free struct
 */
void directive_fep_bridge_destroy(directive_fep_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    if (bridge->base.bio_async_enabled) {
        directive_fep_bridge_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

/* ============================================================================
 * FEP Computation Functions
 * ============================================================================ */

/**
 * @brief Update FEP state
 *
 * WHAT: Recompute free energy metrics
 * WHY:  Track alignment between directives and FEP
 * HOW:  Update averages using exponential moving average
 */
int directive_fep_bridge_update(directive_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_FEP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update average free energy (EMA) */
    if (bridge->stats.total_actions_evaluated > 0) {
        bridge->stats.avg_free_energy =
            (1.0f - EMA_ALPHA) * bridge->stats.avg_free_energy +
            EMA_ALPHA * bridge->state.free_energy;

        bridge->stats.avg_prediction_error =
            (1.0f - EMA_ALPHA) * bridge->stats.avg_prediction_error +
            EMA_ALPHA * bridge->state.prediction_error;

        bridge->stats.avg_surprise =
            (1.0f - EMA_ALPHA) * bridge->stats.avg_surprise +
            EMA_ALPHA * bridge->state.surprise;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * @brief Compute free energy for action-result pair
 *
 * WHAT: Calculate free energy given action outcome
 * WHY:  Quantify alignment between prediction and reality
 * HOW:  G = prediction_error * precision + surprise
 */
int directive_fep_bridge_compute_free_energy(
    directive_fep_bridge_t* bridge,
    uint32_t action,
    uint32_t result,
    float* free_energy_out
) {
    if (!bridge || !free_energy_out) {
        NIMCP_FEP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Blocked action = high free energy */
    if (result == 1) {  /* Blocked */
        *free_energy_out = FREE_ENERGY_BLOCKED_PENALTY *
                           bridge->state.precision;
    } else {  /* Allowed */
        *free_energy_out = FREE_ENERGY_ALLOWED_BASE;
    }

    /* Weight by prediction error */
    *free_energy_out *= (1.0f + bridge->config.prediction_error_weight *
                         bridge->state.prediction_error);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * @brief Compute expected outcome for action
 *
 * WHAT: Predict whether action will be blocked
 * WHY:  Active inference requires outcome prediction
 * HOW:  Use historical block rate as prior probability
 */
int directive_fep_bridge_compute_expected_outcome(
    directive_fep_bridge_t* bridge,
    uint32_t action,
    bool* expected_blocked
) {
    if (!bridge || !expected_blocked) {
        NIMCP_FEP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Use historical block rate as prediction */
    if (bridge->stats.total_actions_evaluated > 0) {
        float block_rate = (float)bridge->stats.actions_blocked_count /
                          (float)bridge->stats.total_actions_evaluated;

        /* Predict blocked if block rate > 50% */
        *expected_blocked = (block_rate > 0.5f);
    } else {
        /* Default: expect allowed (optimistic prior) */
        *expected_blocked = false;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * @brief Update precision for directive predictions
 *
 * WHAT: Adjust confidence in predictions
 * WHY:  Precision-weighting modulates prediction error influence
 * HOW:  Clamp to [0,1] and update state
 */
int directive_fep_bridge_update_precision(
    directive_fep_bridge_t* bridge,
    float precision
) {
    if (!bridge) {
        NIMCP_FEP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    /* Clamp precision to valid range */
    if (precision < 0.0f) precision = 0.0f;
    if (precision > 1.0f) precision = 1.0f;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state.precision = precision;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Event Handler Functions
 * ============================================================================ */

/**
 * @brief Handle blocked action event
 *
 * WHAT: Update FEP state when action is blocked
 * WHY:  Blocked action = high prediction error and surprise
 * HOW:  Increment counters, update free energy and surprise
 */
int directive_fep_bridge_on_action_blocked(
    directive_fep_bridge_t* bridge,
    uint32_t action,
    uint32_t reason
) {
    if (!bridge) {
        NIMCP_FEP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update counters */
    bridge->stats.total_actions_evaluated++;
    bridge->stats.actions_blocked_count++;

    /* Compute prediction error (unexpected block) */
    bool expected_blocked = false;
    if (bridge->stats.total_actions_evaluated > 1) {
        float block_rate = (float)(bridge->stats.actions_blocked_count - 1) /
                          (float)(bridge->stats.total_actions_evaluated - 1);
        expected_blocked = (block_rate > 0.5f);
    }

    /* High prediction error if we didn't expect blocking */
    if (!expected_blocked) {
        bridge->state.prediction_error += PREDICTION_ERROR_SCALE *
                                          bridge->config.prediction_error_weight;
    } else {
        /* Low prediction error if we expected blocking */
        bridge->state.prediction_error *= 0.9f;
    }

    /* High surprise for blocked action */
    bridge->state.surprise = SURPRISE_BLOCKED_BASE;
    if (bridge->state.surprise > bridge->config.surprise_threshold) {
        bridge->stats.high_surprise_count++;
    }

    /* Recompute free energy */
    bridge->state.free_energy = FREE_ENERGY_BLOCKED_PENALTY *
                                bridge->state.precision *
                                (1.0f + bridge->state.prediction_error);

    NIMCP_LOGGING_DEBUG(LOG_MODULE_DIRECTIVE_FEP
        " Action %u blocked (reason=%u), PE=%.2f, S=%.2f, FE=%.2f",
        action, reason, bridge->state.prediction_error,
        bridge->state.surprise, bridge->state.free_energy);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * @brief Handle allowed action event
 *
 * WHAT: Update FEP state when action is allowed
 * WHY:  Allowed action = low free energy and prediction error
 * HOW:  Increment counters, reduce prediction error and surprise
 */
int directive_fep_bridge_on_action_allowed(
    directive_fep_bridge_t* bridge,
    uint32_t action
) {
    if (!bridge) {
        NIMCP_FEP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update counters */
    bridge->stats.total_actions_evaluated++;
    bridge->stats.actions_allowed_count++;

    /* Low surprise for allowed action */
    bridge->state.surprise = SURPRISE_ALLOWED_BASE;

    /* Decay prediction error (things are as expected) */
    bridge->state.prediction_error *= 0.9f;

    /* Low free energy for allowed action */
    bridge->state.free_energy = FREE_ENERGY_ALLOWED_BASE *
                                (1.0f + 0.1f * bridge->state.prediction_error);

    NIMCP_LOGGING_DEBUG(LOG_MODULE_DIRECTIVE_FEP
        " Action %u allowed, PE=%.2f, S=%.2f, FE=%.2f",
        action, bridge->state.prediction_error,
        bridge->state.surprise, bridge->state.free_energy);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Query Functions
 * ============================================================================ */

/**
 * @brief Get current FEP state
 *
 * WHAT: Retrieve current free energy metrics
 * WHY:  Monitoring and debugging
 * HOW:  Copy state under mutex protection
 */
int directive_fep_bridge_get_state(
    const directive_fep_bridge_t* bridge,
    directive_fep_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_FEP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/**
 * @brief Get statistics
 *
 * WHAT: Retrieve cumulative statistics
 * WHY:  Monitor effectiveness of integration
 * HOW:  Copy stats under mutex protection
 */
int directive_fep_bridge_get_stats(
    const directive_fep_bridge_t* bridge,
    directive_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_FEP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
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
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async
 * WHY:  Enable cross-module messaging
 * HOW:  Register with BIO_MODULE_FEP_CORE_DIRECTIVES
 */
int directive_fep_bridge_connect_bio_async(directive_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_FEP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    if (bridge->base.bio_async_enabled) {
        return 0;  /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_CORE_DIRECTIVES,
        .module_name = "directive_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO(LOG_MODULE_DIRECTIVE_FEP " Connected to bio-async");
        return 0;
    }

    NIMCP_LOGGING_WARN(LOG_MODULE_DIRECTIVE_FEP " Failed to connect bio-async");
    NIMCP_FEP_CHECK_THROW(false, NIMCP_ERROR_INVALID_STATE, "invalid state");
}

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Cleanup before shutdown
 * HOW:  Unregister module, clear flag
 */
int directive_fep_bridge_disconnect_bio_async(directive_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_FEP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    if (!bridge->base.bio_async_enabled) {
        return 0;  /* Already disconnected */
    }

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO(LOG_MODULE_DIRECTIVE_FEP " Disconnected from bio-async");

    return 0;
}

/**
 * @brief Check if bio-async is connected
 *
 * WHAT: Query bio-async connection status
 * WHY:  Conditional bio-async operations
 * HOW:  Return flag value
 */
bool directive_fep_bridge_is_bio_async_connected(
    const directive_fep_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}
