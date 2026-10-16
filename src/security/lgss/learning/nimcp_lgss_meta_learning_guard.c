/**
 * @file nimcp_lgss_meta_learning_guard.c
 * @brief Implementation of LGSS Meta-Learning Guard
 * @version 1.0.0
 * @date 2026-01-16
 *
 * Implements the meta-learning guard that constrains learning-to-learn updates
 * to prevent modification of safety-critical learning mechanisms and ensure
 * stable meta-level dynamics.
 */

#include "security/lgss/learning/nimcp_lgss_meta_learning_guard.h"
#include "utils/validation/nimcp_common.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(lgss_meta_learning_guard, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

/** Parameter registry entry */
typedef struct {
    meta_param_info_t info;
    bool registered;

    /* Update history for oscillation detection */
    float* update_history;       /**< Recent update deltas */
    uint32_t history_head;       /**< Circular buffer head */
    uint32_t history_count;      /**< Number of history entries */

    /* Cumulative tracking */
    float cumulative_change;     /**< Total change since registration */
    uint64_t update_count;       /**< Number of updates to this param */
    uint64_t last_update_time;   /**< Timestamp of last update */
} param_registry_entry_t;

/** Sliding window for rate limiting */
typedef struct {
    uint64_t* timestamps;       /**< Circular buffer of update timestamps */
    uint32_t capacity;          /**< Buffer capacity */
    uint32_t head;              /**< Write position */
    uint32_t count;             /**< Current count */
} sliding_window_t;

/** Stability tracking */
typedef struct {
    float* eigenvalue_history;           /**< History of max eigenvalues */
    uint32_t eigenvalue_head;            /**< Circular buffer head */
    uint32_t eigenvalue_count;           /**< Number of entries */
    uint32_t eigenvalue_capacity;        /**< Buffer capacity */

    float current_eigenvalue;            /**< Current max eigenvalue estimate */
    float eigenvalue_ewma;               /**< EWMA of eigenvalues */
    meta_stability_state_t current_state; /**< Current stability state */
} stability_tracker_t;

/** Meta-drift tracking */
typedef struct {
    float current_drift;                 /**< Current drift magnitude */
    float drift_ewma;                    /**< EWMA of drift */
    float drift_rate;                    /**< Current drift rate */
    uint64_t drift_samples;              /**< Number of samples */
} drift_tracker_t;

/** Internal meta-learning guard structure */
struct meta_learning_guard_internal {
    uint32_t magic;                         /**< Magic number for validation */

    /* Configuration */
    meta_learning_guard_config_t config;

    /* Parameter registry */
    param_registry_entry_t* params;         /**< Array of registered parameters */
    uint32_t param_capacity;                /**< Registry capacity */
    uint32_t param_count;                   /**< Number of registered params */

    /* Frozen parameter set (using simple array for fast lookup) */
    bool* frozen_flags;                     /**< Quick lookup for frozen status */
    uint32_t frozen_count;                  /**< Number of frozen params */

    /* Rate limiting */
    sliding_window_t rate_window;           /**< Sliding window for rate limiting */

    /* Stability tracking */
    stability_tracker_t stability;

    /* Meta-drift tracking */
    drift_tracker_t drift;

    /* Statistics */
    meta_learning_guard_stats_t stats;

    /* Security orchestrator */
    security_orchestrator_t orchestrator;

    /* Oscillation detection history */
    uint32_t oscillation_window_size;
};

/* ============================================================================
 * INTERNAL HELPERS
 * ============================================================================ */

/**
 * Get current time in microseconds
 */
static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * Initialize sliding window
 */
static bool sliding_window_init(sliding_window_t* window, uint32_t capacity) {
    window->timestamps = nimcp_calloc(capacity, sizeof(uint64_t));
    if (!window->timestamps) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sliding_window_init: window->timestamps is NULL");
        return false;
    }
    window->capacity = capacity;
    window->head = 0;
    window->count = 0;
    return true;
}

/**
 * Clean up sliding window
 */
static void sliding_window_destroy(sliding_window_t* window) {
    if (window->timestamps) {
        nimcp_free(window->timestamps);
        window->timestamps = NULL;
    }
}

/**
 * Add timestamp to sliding window
 */
static void sliding_window_add(sliding_window_t* window, uint64_t timestamp) {
    window->timestamps[window->head] = timestamp;
    window->head = (window->head + 1) % window->capacity;
    if (window->count < window->capacity) {
        window->count++;
    }
}

/**
 * Count entries within time window
 */
static uint32_t sliding_window_count_within(sliding_window_t* window,
                                            uint64_t current_time,
                                            uint64_t window_us) {
    uint32_t count = 0;
    uint64_t cutoff = current_time > window_us ? current_time - window_us : 0;

    for (uint32_t i = 0; i < window->count; i++) {
        if (window->timestamps[i] >= cutoff) {
            count++;
        }
    }
    return count;
}

/**
 * Initialize stability tracker
 */
static bool stability_tracker_init(stability_tracker_t* tracker, uint32_t capacity) {
    tracker->eigenvalue_history = nimcp_calloc(capacity, sizeof(float));
    if (!tracker->eigenvalue_history) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stability_tracker_init: tracker->eigenvalue_history is NULL");
        return false;
    }
    tracker->eigenvalue_capacity = capacity;
    tracker->eigenvalue_head = 0;
    tracker->eigenvalue_count = 0;
    tracker->current_eigenvalue = 0.0f;
    tracker->eigenvalue_ewma = 0.0f;
    tracker->current_state = META_STABILITY_STABLE;
    return true;
}

/**
 * Clean up stability tracker
 */
static void stability_tracker_destroy(stability_tracker_t* tracker) {
    if (tracker->eigenvalue_history) {
        nimcp_free(tracker->eigenvalue_history);
        tracker->eigenvalue_history = NULL;
    }
}

/**
 * Add eigenvalue to stability history
 */
static void stability_tracker_add(stability_tracker_t* tracker, float eigenvalue, float alpha) {
    tracker->eigenvalue_history[tracker->eigenvalue_head] = eigenvalue;
    tracker->eigenvalue_head = (tracker->eigenvalue_head + 1) % tracker->eigenvalue_capacity;
    if (tracker->eigenvalue_count < tracker->eigenvalue_capacity) {
        tracker->eigenvalue_count++;
    }

    tracker->current_eigenvalue = eigenvalue;
    tracker->eigenvalue_ewma = alpha * eigenvalue + (1.0f - alpha) * tracker->eigenvalue_ewma;
}

/**
 * Assess stability based on eigenvalue history
 */
static meta_stability_state_t assess_stability(stability_tracker_t* tracker, float threshold) {
    if (tracker->eigenvalue_count < 3) {
        return META_STABILITY_UNKNOWN;
    }

    /* Check for diverging (eigenvalue > 1 consistently) */
    if (tracker->eigenvalue_ewma > threshold) {
        return META_STABILITY_DIVERGING;
    }

    /* Check for oscillation - look at variance in recent eigenvalues */
    float sum = 0.0f;
    float sum_sq = 0.0f;
    uint32_t count = tracker->eigenvalue_count > 10 ? 10 : tracker->eigenvalue_count;
    uint32_t start = tracker->eigenvalue_head >= count ?
                     tracker->eigenvalue_head - count :
                     tracker->eigenvalue_capacity - (count - tracker->eigenvalue_head);

    for (uint32_t i = 0; i < count; i++) {
        uint32_t idx = (start + i) % tracker->eigenvalue_capacity;
        sum += tracker->eigenvalue_history[idx];
        sum_sq += tracker->eigenvalue_history[idx] * tracker->eigenvalue_history[idx];
    }

    float mean = sum / count;
    float variance = (sum_sq / count) - (mean * mean);

    /* High variance indicates oscillation */
    if (variance > 0.1f) {
        return META_STABILITY_OSCILLATING;
    }

    /* Close to threshold is marginal */
    if (tracker->eigenvalue_ewma > threshold * 0.8f) {
        return META_STABILITY_MARGINAL;
    }

    return META_STABILITY_STABLE;
}

/**
 * Find parameter by ID
 */
static param_registry_entry_t* find_param(struct meta_learning_guard_internal* g, uint32_t param_id) {
    if (param_id >= g->param_capacity) { return NULL; }
    if (!g->params[param_id].registered) {
        return NULL;
    }
    return &g->params[param_id];
}

/**
 * Detect oscillation in parameter updates
 */
static bool detect_oscillation(param_registry_entry_t* param, uint32_t window_size, float threshold) {
    if (param->history_count < window_size) {
        return false;
    }

    /* Count sign changes in recent updates */
    uint32_t sign_changes = 0;
    int last_sign = 0;

    uint32_t start = param->history_head >= window_size ?
                     param->history_head - window_size :
                     META_STABILITY_HISTORY_SIZE - (window_size - param->history_head);

    for (uint32_t i = 0; i < window_size; i++) {
        uint32_t idx = (start + i) % META_STABILITY_HISTORY_SIZE;
        float delta = param->update_history[idx];

        int current_sign = (delta > 0.001f) ? 1 : ((delta < -0.001f) ? -1 : 0);

        if (current_sign != 0 && last_sign != 0 && current_sign != last_sign) {
            sign_changes++;
        }
        if (current_sign != 0) {
            last_sign = current_sign;
        }
    }

    /* Many sign changes indicates oscillation */
    float oscillation_ratio = (float)sign_changes / (float)(window_size - 1);
    return oscillation_ratio > threshold;
}

/**
 * Calculate total meta-drift from initial values
 */
static float calculate_total_drift(struct meta_learning_guard_internal* g) {
    float total_drift = 0.0f;

    for (uint32_t i = 0; i < g->param_capacity; i++) {
        if (g->params[i].registered) {
            float diff = g->params[i].info.current_value - g->params[i].info.initial_value;
            total_drift += diff * diff;
        }
    }

    return sqrtf(total_drift);
}

/* ============================================================================
 * PUBLIC API IMPLEMENTATION
 * ============================================================================ */

meta_learning_guard_config_t meta_learning_guard_default_config(void) {
    meta_learning_guard_config_t config = {
        /* Update constraints */
        .max_update_magnitude = META_DEFAULT_UPDATE_LIMIT,
        .max_cumulative_update = 1.0f,
        .max_updates_per_second = 100,
        .update_window_sec = 1.0f,

        /* Stability monitoring */
        .enable_stability_monitoring = true,
        .stability_threshold = META_DEFAULT_STABILITY_THRESHOLD,
        .stability_window_size = META_STABILITY_HISTORY_SIZE,
        .oscillation_threshold = 0.5f,

        /* Meta-drift detection */
        .enable_drift_detection = true,
        .drift_threshold = META_DEFAULT_DRIFT_THRESHOLD,
        .drift_ewma_alpha = 0.1f,

        /* Safety parameter protection */
        .protect_safety_params = true,
        .safety_param_update_limit = 0.01f,

        /* Hierarchical approval */
        .enable_hierarchical_approval = true,
        .base_approval_level = 0,
        .approval_threshold_magnitude = 0.05f,

        /* Bounds enforcement */
        .enforce_bounds = true,
        .clamp_to_bounds = true,

        /* Oscillation prevention */
        .enable_oscillation_prevention = true,
        .oscillation_window_size = 10,

        /* Logging and monitoring */
        .enable_violation_logging = true,
        .enable_statistics = true
    };
    return config;
}

meta_learning_guard_t meta_learning_guard_create(
    const meta_learning_guard_config_t* config,
    security_orchestrator_t orchestrator
) {
    struct meta_learning_guard_internal* guard = nimcp_calloc(1, sizeof(*guard));
    if (!guard) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "meta_learning_guard_create: allocation failed");

        return NULL;
    }

    guard->magic = LGSS_META_LEARNING_GUARD_MAGIC;

    /* Copy configuration */
    if (config) {
        guard->config = *config;
    } else {
        guard->config = meta_learning_guard_default_config();
    }

    /* Initialize parameter registry */
    guard->param_capacity = META_MAX_PARAMETERS;
    guard->params = nimcp_calloc(guard->param_capacity, sizeof(param_registry_entry_t));
    if (!guard->params) {
        nimcp_free(guard);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "meta_learning_guard_create: guard->params is NULL");
        return NULL;
    }

    /* Allocate update history for each potential parameter */
    for (uint32_t i = 0; i < guard->param_capacity; i++) {
        guard->params[i].update_history = nimcp_calloc(META_STABILITY_HISTORY_SIZE, sizeof(float));
        if (!guard->params[i].update_history) {
            /* Cleanup already allocated histories */
            for (uint32_t j = 0; j < i; j++) {
                nimcp_free(guard->params[j].update_history);
            }
            nimcp_free(guard->params);
            nimcp_free(guard);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "meta_learning_guard_create: update_history allocation failed");
            return NULL;
        }
    }

    /* Initialize frozen flags */
    guard->frozen_flags = nimcp_calloc(guard->param_capacity, sizeof(bool));
    if (!guard->frozen_flags) {
        for (uint32_t i = 0; i < guard->param_capacity; i++) {
            nimcp_free(guard->params[i].update_history);
        }
        nimcp_free(guard->params);
        nimcp_free(guard);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "meta_learning_guard_create: frozen_flags allocation failed");
        return NULL;
    }

    /* Initialize rate limiting sliding window */
    uint32_t window_capacity = (uint32_t)(guard->config.max_updates_per_second *
                                          guard->config.update_window_sec * 2);
    if (window_capacity < 100) window_capacity = 100;
    if (!sliding_window_init(&guard->rate_window, window_capacity)) {
        nimcp_free(guard->frozen_flags);
        for (uint32_t i = 0; i < guard->param_capacity; i++) {
            nimcp_free(guard->params[i].update_history);
        }
        nimcp_free(guard->params);
        nimcp_free(guard);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "meta_learning_guard_create: sliding_window_init is NULL");
        return NULL;
    }

    /* Initialize stability tracking */
    if (!stability_tracker_init(&guard->stability, META_EIGENVALUE_HISTORY_SIZE)) {
        sliding_window_destroy(&guard->rate_window);
        nimcp_free(guard->frozen_flags);
        for (uint32_t i = 0; i < guard->param_capacity; i++) {
            nimcp_free(guard->params[i].update_history);
        }
        nimcp_free(guard->params);
        nimcp_free(guard);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "meta_learning_guard_create: stability_tracker_init is NULL");
        return NULL;
    }

    /* Initialize oscillation window size */
    guard->oscillation_window_size = guard->config.oscillation_window_size;
    if (guard->oscillation_window_size > META_STABILITY_HISTORY_SIZE) {
        guard->oscillation_window_size = META_STABILITY_HISTORY_SIZE;
    }

    /* Store orchestrator reference */
    guard->orchestrator = orchestrator;

    return guard;
}

void meta_learning_guard_destroy(meta_learning_guard_t guard) {
    if (!guard) return;

    struct meta_learning_guard_internal* g = guard;
    if (g->magic != LGSS_META_LEARNING_GUARD_MAGIC) return;

    stability_tracker_destroy(&g->stability);
    sliding_window_destroy(&g->rate_window);
    nimcp_free(g->frozen_flags);

    for (uint32_t i = 0; i < g->param_capacity; i++) {
        if (g->params[i].update_history) {
            nimcp_free(g->params[i].update_history);
        }
    }
    nimcp_free(g->params);

    g->magic = 0;
    nimcp_free(g);
}

int meta_learning_guard_reset(meta_learning_guard_t guard) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");

    struct meta_learning_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_META_LEARNING_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE,
                     "invalid guard magic");

    /* Reset rate window */
    g->rate_window.head = 0;
    g->rate_window.count = 0;

    /* Reset stability tracking */
    g->stability.eigenvalue_head = 0;
    g->stability.eigenvalue_count = 0;
    g->stability.current_eigenvalue = 0.0f;
    g->stability.eigenvalue_ewma = 0.0f;
    g->stability.current_state = META_STABILITY_STABLE;

    /* Reset drift tracking */
    g->drift.current_drift = 0.0f;
    g->drift.drift_ewma = 0.0f;
    g->drift.drift_rate = 0.0f;
    g->drift.drift_samples = 0;

    /* Reset parameter histories (but not registrations) */
    for (uint32_t i = 0; i < g->param_capacity; i++) {
        if (g->params[i].registered) {
            g->params[i].history_head = 0;
            g->params[i].history_count = 0;
            g->params[i].cumulative_change = 0.0f;
            g->params[i].update_count = 0;
            /* Reset current to initial */
            g->params[i].info.current_value = g->params[i].info.initial_value;
        }
    }

    /* Reset statistics */
    memset(&g->stats, 0, sizeof(g->stats));

    return NIMCP_SUCCESS;
}

int meta_learning_guard_register_param(
    meta_learning_guard_t guard,
    const meta_param_info_t* info
) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");
    NIMCP_CHECK_THROW(info, NIMCP_ERROR_NULL_POINTER, "info is NULL");

    struct meta_learning_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_META_LEARNING_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE,
                     "invalid guard magic");

    NIMCP_CHECK_THROW(info->param_id < g->param_capacity, NIMCP_ERROR_OUT_OF_RANGE,
                     "param_id %u out of range", info->param_id);

    NIMCP_CHECK_THROW(!g->params[info->param_id].registered, NIMCP_ERROR_ALREADY_EXISTS,
                     "param_id %u already registered", info->param_id);

    /* Copy parameter info */
    g->params[info->param_id].info = *info;
    g->params[info->param_id].registered = true;
    g->params[info->param_id].history_head = 0;
    g->params[info->param_id].history_count = 0;
    g->params[info->param_id].cumulative_change = 0.0f;
    g->params[info->param_id].update_count = 0;
    g->params[info->param_id].last_update_time = 0;

    g->param_count++;
    g->stats.total_params_tracked = g->param_count;

    return NIMCP_SUCCESS;
}

int meta_learning_guard_unregister_param(
    meta_learning_guard_t guard,
    uint32_t param_id
) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");

    struct meta_learning_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_META_LEARNING_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE,
                     "invalid guard magic");

    NIMCP_CHECK_THROW(param_id < g->param_capacity, NIMCP_ERROR_OUT_OF_RANGE,
                     "param_id %u out of range", param_id);

    NIMCP_CHECK_THROW(g->params[param_id].registered, NIMCP_ERROR_NOT_FOUND,
                     "param_id %u not registered", param_id);

    /* Clear registration */
    g->params[param_id].registered = false;
    g->frozen_flags[param_id] = false;
    g->param_count--;
    g->stats.total_params_tracked = g->param_count;

    return NIMCP_SUCCESS;
}

int meta_learning_guard_get_param_info(
    meta_learning_guard_t guard,
    uint32_t param_id,
    meta_param_info_t* info
) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");
    NIMCP_CHECK_THROW(info, NIMCP_ERROR_NULL_POINTER, "info is NULL");

    struct meta_learning_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_META_LEARNING_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE,
                     "invalid guard magic");

    param_registry_entry_t* param = find_param(g, param_id);
    NIMCP_CHECK_THROW(param, NIMCP_ERROR_NOT_FOUND, "param_id %u not found", param_id);

    *info = param->info;
    return NIMCP_SUCCESS;
}

int meta_learning_guard_propose_update(
    meta_learning_guard_t guard,
    const meta_update_proposal_t* proposal,
    meta_update_result_t* result
) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");
    NIMCP_CHECK_THROW(proposal, NIMCP_ERROR_NULL_POINTER, "proposal is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_NULL_POINTER, "result is NULL");

    struct meta_learning_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_META_LEARNING_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE,
                     "invalid guard magic");

    uint64_t start_time = get_time_us();

    /* Initialize result */
    memset(result, 0, sizeof(*result));
    result->allowed = true;
    result->stability = META_STABILITY_UNKNOWN;

    /* Update statistics */
    g->stats.total_proposals++;

    /* Find parameter */
    param_registry_entry_t* param = find_param(g, proposal->param_id);
    if (!param) {
        result->allowed = false;
        snprintf(result->reason, sizeof(result->reason),
                "Parameter %u not registered", proposal->param_id);
        g->stats.proposals_blocked++;
        return NIMCP_SUCCESS;
    }

    float current_value = param->info.current_value;
    float proposed_change = proposal->proposed_value - current_value;
    result->original_change = proposed_change;
    result->adjusted_value = proposal->proposed_value;
    result->allowed_change = proposed_change;

    /* Check 1: Frozen parameter */
    if (g->frozen_flags[proposal->param_id]) {
        result->violations |= META_VIOLATION_FROZEN_PARAM;
        result->allowed = false;
        snprintf(result->reason, sizeof(result->reason),
                "Parameter %u is frozen", proposal->param_id);
        g->stats.frozen_violations++;
        g->stats.proposals_blocked++;
        return NIMCP_SUCCESS;
    }

    /* Check 2: Safety parameter protection */
    if (g->config.protect_safety_params && param->info.type == META_PARAM_SAFETY) {
        if (fabsf(proposed_change) > g->config.safety_param_update_limit) {
            result->violations |= META_VIOLATION_SAFETY_PARAM;
            result->allowed = false;
            snprintf(result->reason, sizeof(result->reason),
                    "Safety parameter update magnitude %.4f exceeds limit %.4f",
                    fabsf(proposed_change), g->config.safety_param_update_limit);
            g->stats.safety_param_violations++;
            g->stats.proposals_blocked++;
            return NIMCP_SUCCESS;
        }
    }

    /* Check 3: Update magnitude */
    float magnitude_limit = param->info.max_change_per_update > 0 ?
                           param->info.max_change_per_update :
                           g->config.max_update_magnitude;

    if (fabsf(proposed_change) > magnitude_limit) {
        result->violations |= META_VIOLATION_MAGNITUDE;

        if (g->config.clamp_to_bounds) {
            /* Clamp the update */
            float sign = proposed_change >= 0 ? 1.0f : -1.0f;
            result->allowed_change = sign * magnitude_limit;
            result->adjusted_value = current_value + result->allowed_change;
            g->stats.proposals_clamped++;
        } else {
            result->allowed = false;
            snprintf(result->reason, sizeof(result->reason),
                    "Update magnitude %.4f exceeds limit %.4f",
                    fabsf(proposed_change), magnitude_limit);
            g->stats.magnitude_violations++;
            g->stats.proposals_blocked++;
            return NIMCP_SUCCESS;
        }
    }

    /* Check 4: Bounds enforcement */
    if (g->config.enforce_bounds) {
        if (result->adjusted_value < param->info.min_value) {
            result->violations |= META_VIOLATION_BOUNDS;
            if (g->config.clamp_to_bounds) {
                result->adjusted_value = param->info.min_value;
                result->allowed_change = result->adjusted_value - current_value;
                g->stats.proposals_clamped++;
            } else {
                result->allowed = false;
                snprintf(result->reason, sizeof(result->reason),
                        "Value %.4f below minimum %.4f",
                        result->adjusted_value, param->info.min_value);
                g->stats.bounds_violations++;
                g->stats.proposals_blocked++;
                return NIMCP_SUCCESS;
            }
        }
        if (result->adjusted_value > param->info.max_value) {
            result->violations |= META_VIOLATION_BOUNDS;
            if (g->config.clamp_to_bounds) {
                result->adjusted_value = param->info.max_value;
                result->allowed_change = result->adjusted_value - current_value;
                g->stats.proposals_clamped++;
            } else {
                result->allowed = false;
                snprintf(result->reason, sizeof(result->reason),
                        "Value %.4f above maximum %.4f",
                        result->adjusted_value, param->info.max_value);
                g->stats.bounds_violations++;
                g->stats.proposals_blocked++;
                return NIMCP_SUCCESS;
            }
        }
    }

    /* Check 5: Rate limiting */
    uint64_t current_time = get_time_us();
    uint64_t window_us = (uint64_t)(g->config.update_window_sec * 1000000.0f);
    uint32_t recent_updates = sliding_window_count_within(&g->rate_window, current_time, window_us);

    if (recent_updates >= g->config.max_updates_per_second) {
        result->violations |= META_VIOLATION_RATE_LIMIT;
        result->allowed = false;
        snprintf(result->reason, sizeof(result->reason),
                "Rate limit exceeded: %u updates in window", recent_updates);
        g->stats.rate_limit_violations++;
        g->stats.proposals_blocked++;
        return NIMCP_SUCCESS;
    }

    /* Check 6: Stability analysis */
    if (g->config.enable_stability_monitoring) {
        result->stability = g->stability.current_state;

        if (g->stability.current_state == META_STABILITY_DIVERGING) {
            result->violations |= META_VIOLATION_STABILITY;
            result->allowed = false;
            snprintf(result->reason, sizeof(result->reason),
                    "Meta-learning is diverging (eigenvalue: %.4f)",
                    g->stability.current_eigenvalue);
            g->stats.stability_violations++;
            g->stats.proposals_blocked++;
            return NIMCP_SUCCESS;
        }

        if (g->stability.current_state == META_STABILITY_OSCILLATING) {
            /* Allow but flag */
            result->violations |= META_VIOLATION_STABILITY;
        }
    }

    /* Check 7: Meta-drift detection */
    if (g->config.enable_drift_detection) {
        float new_drift = calculate_total_drift(g);
        /* Estimate what drift would be after this update */
        float diff = result->adjusted_value - param->info.initial_value;
        float old_diff = current_value - param->info.initial_value;
        float drift_contribution = (diff * diff) - (old_diff * old_diff);
        float predicted_drift = sqrtf(new_drift * new_drift + drift_contribution);

        if (predicted_drift > g->config.drift_threshold) {
            result->violations |= META_VIOLATION_DRIFT;
            result->allowed = false;
            snprintf(result->reason, sizeof(result->reason),
                    "Meta-drift would exceed threshold (%.4f > %.4f)",
                    predicted_drift, g->config.drift_threshold);
            g->stats.drift_violations++;
            g->stats.proposals_blocked++;
            return NIMCP_SUCCESS;
        }
    }

    /* Check 8: Hierarchical approval */
    if (g->config.enable_hierarchical_approval) {
        uint32_t required_level = param->info.requires_approval ?
                                  param->info.approval_level :
                                  g->config.base_approval_level;

        /* Large updates require higher approval */
        if (fabsf(result->allowed_change) > g->config.approval_threshold_magnitude) {
            required_level++;
        }

        if (proposal->approval_level < required_level) {
            result->violations |= META_VIOLATION_APPROVAL_REQUIRED;
            result->requires_higher_approval = true;
            result->required_approval_level = required_level;
            result->allowed = false;
            snprintf(result->reason, sizeof(result->reason),
                    "Requires approval level %u (have %u)",
                    required_level, proposal->approval_level);
            g->stats.proposals_blocked++;
            return NIMCP_SUCCESS;
        }
    }

    /* Check 9: Oscillation detection */
    if (g->config.enable_oscillation_prevention && param->history_count >= g->oscillation_window_size) {
        if (detect_oscillation(param, g->oscillation_window_size, g->config.oscillation_threshold)) {
            result->violations |= META_VIOLATION_OSCILLATION;
            /* Allow but with reduced magnitude */
            result->allowed_change *= 0.5f;
            result->adjusted_value = current_value + result->allowed_change;
            g->stats.oscillation_detections++;
        }
    }

    /* Update statistics for allowed proposals */
    if (result->allowed) {
        g->stats.proposals_allowed++;
    }

    /* Track timing */
    uint64_t elapsed = get_time_us() - start_time;
    g->stats.guard_overhead_us += elapsed;
    g->stats.avg_validation_time_us = (float)g->stats.guard_overhead_us /
                                       (float)g->stats.total_proposals;

    return NIMCP_SUCCESS;
}

int meta_learning_guard_commit_update(
    meta_learning_guard_t guard,
    uint32_t param_id,
    float new_value
) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");

    struct meta_learning_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_META_LEARNING_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE,
                     "invalid guard magic");

    param_registry_entry_t* param = find_param(g, param_id);
    NIMCP_CHECK_THROW(param, NIMCP_ERROR_NOT_FOUND, "param_id %u not found", param_id);

    uint64_t current_time = get_time_us();

    /* Calculate change */
    float old_value = param->info.current_value;
    float change = new_value - old_value;

    /* Update parameter value */
    param->info.current_value = new_value;

    /* Add to update history */
    param->update_history[param->history_head] = change;
    param->history_head = (param->history_head + 1) % META_STABILITY_HISTORY_SIZE;
    if (param->history_count < META_STABILITY_HISTORY_SIZE) {
        param->history_count++;
    }

    /* Update cumulative tracking */
    param->cumulative_change += fabsf(change);
    param->update_count++;
    param->last_update_time = current_time;

    /* Update rate limiting window */
    sliding_window_add(&g->rate_window, current_time);

    /* Update drift tracking */
    g->drift.current_drift = calculate_total_drift(g);
    g->drift.drift_ewma = g->config.drift_ewma_alpha * g->drift.current_drift +
                          (1.0f - g->config.drift_ewma_alpha) * g->drift.drift_ewma;
    g->drift.drift_samples++;

    /* Update statistics */
    float magnitude = fabsf(change);
    g->stats.avg_update_magnitude = (g->stats.avg_update_magnitude *
                                     (g->stats.proposals_allowed - 1) + magnitude) /
                                    g->stats.proposals_allowed;
    if (magnitude > g->stats.max_update_magnitude_seen) {
        g->stats.max_update_magnitude_seen = magnitude;
    }

    g->stats.cumulative_drift = g->drift.current_drift;
    if (g->drift.current_drift > g->stats.max_drift_seen) {
        g->stats.max_drift_seen = g->drift.current_drift;
    }

    return NIMCP_SUCCESS;
}

uint32_t meta_learning_guard_propose_batch(
    meta_learning_guard_t guard,
    const meta_update_proposal_t* proposals,
    meta_update_result_t* results,
    uint32_t count
) {
    if (!guard || !proposals || !results || count == 0) {
        return 0;
    }

    uint32_t allowed = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (meta_learning_guard_propose_update(guard, &proposals[i], &results[i]) == 0) {
            if (results[i].allowed) {
                allowed++;
            }
        }
    }
    return allowed;
}

int meta_learning_guard_freeze_param(meta_learning_guard_t guard, uint32_t param_id) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");

    struct meta_learning_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_META_LEARNING_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE,
                     "invalid guard magic");

    NIMCP_CHECK_THROW(param_id < g->param_capacity, NIMCP_ERROR_OUT_OF_RANGE,
                     "param_id %u out of range", param_id);

    NIMCP_CHECK_THROW(g->params[param_id].registered, NIMCP_ERROR_NOT_FOUND,
                     "param_id %u not registered", param_id);

    NIMCP_CHECK_THROW(!g->frozen_flags[param_id], NIMCP_ERROR_ALREADY_EXISTS,
                     "param_id %u already frozen", param_id);

    g->frozen_flags[param_id] = true;
    g->params[param_id].info.is_frozen = true;
    g->frozen_count++;
    g->stats.frozen_param_count = g->frozen_count;

    return NIMCP_SUCCESS;
}

int meta_learning_guard_unfreeze_param(meta_learning_guard_t guard, uint32_t param_id) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");

    struct meta_learning_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_META_LEARNING_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE,
                     "invalid guard magic");

    NIMCP_CHECK_THROW(param_id < g->param_capacity, NIMCP_ERROR_OUT_OF_RANGE,
                     "param_id %u out of range", param_id);

    NIMCP_CHECK_THROW(g->frozen_flags[param_id], NIMCP_ERROR_NOT_FOUND,
                     "param_id %u not frozen", param_id);

    g->frozen_flags[param_id] = false;
    g->params[param_id].info.is_frozen = false;
    g->frozen_count--;
    g->stats.frozen_param_count = g->frozen_count;

    return NIMCP_SUCCESS;
}

bool meta_learning_guard_is_param_frozen(meta_learning_guard_t guard, uint32_t param_id) {
    if (!guard) {
        return false;
    }

    struct meta_learning_guard_internal* g = guard;
    if (g->magic != LGSS_META_LEARNING_GUARD_MAGIC) {
        return false;
    }

    if (param_id >= g->param_capacity) {
        return false;
    }

    return g->frozen_flags[param_id];
}

uint32_t meta_learning_guard_freeze_by_type(
    meta_learning_guard_t guard,
    meta_param_type_t type
) {
    if (!guard) return 0;

    struct meta_learning_guard_internal* g = guard;
    if (g->magic != LGSS_META_LEARNING_GUARD_MAGIC) return 0;

    uint32_t count = 0;
    for (uint32_t i = 0; i < g->param_capacity; i++) {
        if (g->params[i].registered && g->params[i].info.type == type) {
            if (!g->frozen_flags[i]) {
                g->frozen_flags[i] = true;
                g->params[i].info.is_frozen = true;
                g->frozen_count++;
                count++;
            }
        }
    }

    g->stats.frozen_param_count = g->frozen_count;
    return count;
}

int meta_learning_guard_get_stability(
    meta_learning_guard_t guard,
    meta_stability_state_t* state
) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_NULL_POINTER, "state is NULL");

    struct meta_learning_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_META_LEARNING_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE,
                     "invalid guard magic");

    *state = g->stability.current_state;
    return NIMCP_SUCCESS;
}

int meta_learning_guard_update_stability(
    meta_learning_guard_t guard,
    float jacobian_estimate
) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");

    struct meta_learning_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_META_LEARNING_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE,
                     "invalid guard magic");

    /* Add to stability history with EWMA update */
    float alpha = 0.1f;
    stability_tracker_add(&g->stability, jacobian_estimate, alpha);

    /* Assess stability */
    meta_stability_state_t old_state = g->stability.current_state;
    g->stability.current_state = assess_stability(&g->stability, g->config.stability_threshold);

    /* Track state transitions */
    if (old_state != g->stability.current_state) {
        g->stats.stability_transitions++;
    }

    /* Update statistics */
    g->stats.current_stability = g->stability.current_state;
    if (jacobian_estimate > g->stats.max_eigenvalue_seen) {
        g->stats.max_eigenvalue_seen = jacobian_estimate;
    }
    g->stats.avg_eigenvalue = g->stability.eigenvalue_ewma;

    return NIMCP_SUCCESS;
}

int meta_learning_guard_get_eigenvalue(
    meta_learning_guard_t guard,
    float* eigenvalue
) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");
    NIMCP_CHECK_THROW(eigenvalue, NIMCP_ERROR_NULL_POINTER, "eigenvalue is NULL");

    struct meta_learning_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_META_LEARNING_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE,
                     "invalid guard magic");

    *eigenvalue = g->stability.current_eigenvalue;
    return NIMCP_SUCCESS;
}

int meta_learning_guard_predict_stability_impact(
    meta_learning_guard_t guard,
    const meta_update_proposal_t* proposal,
    meta_stability_state_t* predicted_stability
) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");
    NIMCP_CHECK_THROW(proposal, NIMCP_ERROR_NULL_POINTER, "proposal is NULL");
    NIMCP_CHECK_THROW(predicted_stability, NIMCP_ERROR_NULL_POINTER, "predicted_stability is NULL");

    struct meta_learning_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_META_LEARNING_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE,
                     "invalid guard magic");

    /* Simple prediction: larger updates may destabilize */
    param_registry_entry_t* param = find_param(g, proposal->param_id);
    if (!param) {
        *predicted_stability = META_STABILITY_UNKNOWN;
        return NIMCP_SUCCESS;
    }

    float change = fabsf(proposal->proposed_value - param->info.current_value);

    /* If current state is already unstable, any update is risky */
    if (g->stability.current_state == META_STABILITY_DIVERGING) {
        *predicted_stability = META_STABILITY_DIVERGING;
        return NIMCP_SUCCESS;
    }

    if (g->stability.current_state == META_STABILITY_OSCILLATING) {
        /* Large updates during oscillation could push to diverging */
        if (change > g->config.max_update_magnitude * 0.5f) {
            *predicted_stability = META_STABILITY_DIVERGING;
        } else {
            *predicted_stability = META_STABILITY_OSCILLATING;
        }
        return NIMCP_SUCCESS;
    }

    /* Estimate impact based on learning signal and change */
    float impact = change * fabsf(proposal->learning_signal);

    if (impact > g->config.max_update_magnitude * 2.0f) {
        *predicted_stability = META_STABILITY_MARGINAL;
    } else {
        *predicted_stability = g->stability.current_state;
    }

    return NIMCP_SUCCESS;
}

int meta_learning_guard_get_drift(meta_learning_guard_t guard, float* drift) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");
    NIMCP_CHECK_THROW(drift, NIMCP_ERROR_NULL_POINTER, "drift is NULL");

    struct meta_learning_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_META_LEARNING_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE,
                     "invalid guard magic");

    *drift = g->drift.current_drift;
    return NIMCP_SUCCESS;
}

int meta_learning_guard_reset_drift_baseline(meta_learning_guard_t guard) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");

    struct meta_learning_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_META_LEARNING_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE,
                     "invalid guard magic");

    /* Set initial values to current values */
    for (uint32_t i = 0; i < g->param_capacity; i++) {
        if (g->params[i].registered) {
            g->params[i].info.initial_value = g->params[i].info.current_value;
        }
    }

    /* Reset drift tracking */
    g->drift.current_drift = 0.0f;
    g->drift.drift_ewma = 0.0f;
    g->drift.drift_rate = 0.0f;
    g->drift.drift_samples = 0;
    g->stats.cumulative_drift = 0.0f;

    return NIMCP_SUCCESS;
}

int meta_learning_guard_get_drift_rate(
    meta_learning_guard_t guard,
    float* drift_rate
) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");
    NIMCP_CHECK_THROW(drift_rate, NIMCP_ERROR_NULL_POINTER, "drift_rate is NULL");

    struct meta_learning_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_META_LEARNING_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE,
                     "invalid guard magic");

    /* Calculate drift rate from recent history */
    if (g->drift.drift_samples < 2) {
        *drift_rate = 0.0f;
        return NIMCP_SUCCESS;
    }

    *drift_rate = g->drift.drift_rate;
    g->stats.drift_rate = g->drift.drift_rate;
    return NIMCP_SUCCESS;
}

int meta_learning_guard_set_approval_level(
    meta_learning_guard_t guard,
    uint32_t param_id,
    uint32_t approval_level
) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");

    struct meta_learning_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_META_LEARNING_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE,
                     "invalid guard magic");

    param_registry_entry_t* param = find_param(g, param_id);
    NIMCP_CHECK_THROW(param, NIMCP_ERROR_NOT_FOUND, "param_id %u not found", param_id);

    param->info.requires_approval = true;
    param->info.approval_level = approval_level;

    return NIMCP_SUCCESS;
}

int meta_learning_guard_get_approval_level(
    meta_learning_guard_t guard,
    uint32_t param_id,
    uint32_t* approval_level
) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");
    NIMCP_CHECK_THROW(approval_level, NIMCP_ERROR_NULL_POINTER, "approval_level is NULL");

    struct meta_learning_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_META_LEARNING_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE,
                     "invalid guard magic");

    param_registry_entry_t* param = find_param(g, param_id);
    NIMCP_CHECK_THROW(param, NIMCP_ERROR_NOT_FOUND, "param_id %u not found", param_id);

    *approval_level = param->info.requires_approval ? param->info.approval_level : 0;
    return NIMCP_SUCCESS;
}

int meta_learning_guard_get_stats(
    meta_learning_guard_t guard,
    meta_learning_guard_stats_t* stats
) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");

    struct meta_learning_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_META_LEARNING_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE,
                     "invalid guard magic");

    *stats = g->stats;

    /* Calculate current update rate */
    uint64_t current_time = get_time_us();
    uint64_t window_us = (uint64_t)(g->config.update_window_sec * 1000000.0f);
    uint32_t recent = sliding_window_count_within(&g->rate_window, current_time, window_us);
    stats->current_update_rate = (float)recent / g->config.update_window_sec;

    return NIMCP_SUCCESS;
}

int meta_learning_guard_reset_stats(meta_learning_guard_t guard) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");

    struct meta_learning_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_META_LEARNING_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE,
                     "invalid guard magic");

    memset(&g->stats, 0, sizeof(g->stats));
    g->stats.frozen_param_count = g->frozen_count;
    g->stats.total_params_tracked = g->param_count;

    return NIMCP_SUCCESS;
}

const char* meta_param_type_name(meta_param_type_t type) {
    switch (type) {
        case META_PARAM_LEARNING_RATE: return "LEARNING_RATE";
        case META_PARAM_PLASTICITY_RULE: return "PLASTICITY_RULE";
        case META_PARAM_ARCHITECTURE: return "ARCHITECTURE";
        case META_PARAM_OPTIMIZER: return "OPTIMIZER";
        case META_PARAM_EXPLORATION: return "EXPLORATION";
        case META_PARAM_REGULARIZATION: return "REGULARIZATION";
        case META_PARAM_LOSS_WEIGHTS: return "LOSS_WEIGHTS";
        case META_PARAM_ATTENTION: return "ATTENTION";
        case META_PARAM_MEMORY: return "MEMORY";
        case META_PARAM_REWARD_SHAPING: return "REWARD_SHAPING";
        case META_PARAM_SAFETY: return "SAFETY";
        case META_PARAM_CUSTOM: return "CUSTOM";
        default: return "UNKNOWN";
    }
}

const char* meta_violation_name(meta_violation_t violation) {
    switch (violation) {
        case META_VIOLATION_NONE: return "NONE";
        case META_VIOLATION_MAGNITUDE: return "MAGNITUDE";
        case META_VIOLATION_FROZEN_PARAM: return "FROZEN_PARAM";
        case META_VIOLATION_STABILITY: return "STABILITY";
        case META_VIOLATION_DRIFT: return "DRIFT";
        case META_VIOLATION_RATE_LIMIT: return "RATE_LIMIT";
        case META_VIOLATION_SAFETY_PARAM: return "SAFETY_PARAM";
        case META_VIOLATION_BOUNDS: return "BOUNDS";
        case META_VIOLATION_CONSISTENCY: return "CONSISTENCY";
        case META_VIOLATION_APPROVAL_REQUIRED: return "APPROVAL_REQUIRED";
        case META_VIOLATION_OSCILLATION: return "OSCILLATION";
        default: return "UNKNOWN";
    }
}

const char* meta_stability_state_name(meta_stability_state_t state) {
    switch (state) {
        case META_STABILITY_STABLE: return "STABLE";
        case META_STABILITY_MARGINAL: return "MARGINAL";
        case META_STABILITY_OSCILLATING: return "OSCILLATING";
        case META_STABILITY_DIVERGING: return "DIVERGING";
        case META_STABILITY_UNKNOWN: return "UNKNOWN";
        default: return "INVALID";
    }
}

int meta_violations_to_string(
    meta_violation_t violations,
    char* buffer,
    size_t buffer_size
) {
    if (!buffer || buffer_size == 0) return 0;

    buffer[0] = '\0';
    int written = 0;

    if (violations == META_VIOLATION_NONE) {
        return snprintf(buffer, buffer_size, "NONE");
    }

    const char* sep = "";
    for (int i = 0; i < 10; i++) {
        meta_violation_t flag = (meta_violation_t)(1 << i);
        if (violations & flag) {
            int n = snprintf(buffer + written, buffer_size - written,
                           "%s%s", sep, meta_violation_name(flag));
            if (n > 0) {
                written += n;
                sep = "|";
            }
        }
    }

    return written;
}

void meta_learning_guard_print_summary(meta_learning_guard_t guard) {
    if (!guard) {
        printf("Meta-Learning Guard: NULL\n");
        return;
    }

    struct meta_learning_guard_internal* g = guard;
    if (g->magic != LGSS_META_LEARNING_GUARD_MAGIC) {
        printf("Meta-Learning Guard: INVALID (bad magic)\n");
        return;
    }

    printf("=== Meta-Learning Guard Summary ===\n");
    printf("Registered parameters: %u\n", g->param_count);
    printf("Frozen parameters: %u\n", g->frozen_count);
    printf("Stability state: %s\n", meta_stability_state_name(g->stability.current_state));
    printf("Current eigenvalue: %.4f (threshold: %.4f)\n",
           g->stability.current_eigenvalue, g->config.stability_threshold);
    printf("Meta-drift: %.4f / %.4f\n", g->drift.current_drift, g->config.drift_threshold);
    printf("Proposals: %lu total, %lu allowed, %lu blocked, %lu clamped\n",
           (unsigned long)g->stats.total_proposals,
           (unsigned long)g->stats.proposals_allowed,
           (unsigned long)g->stats.proposals_blocked,
           (unsigned long)g->stats.proposals_clamped);

    if (g->stats.stability_violations > 0) {
        printf("Stability violations: %lu\n", (unsigned long)g->stats.stability_violations);
    }
    if (g->stats.drift_violations > 0) {
        printf("Drift violations: %lu\n", (unsigned long)g->stats.drift_violations);
    }
    if (g->stats.oscillation_detections > 0) {
        printf("Oscillation detections: %lu\n", (unsigned long)g->stats.oscillation_detections);
    }

    float rate;
    uint64_t current_time = get_time_us();
    uint64_t window_us = (uint64_t)(g->config.update_window_sec * 1000000.0f);
    uint32_t recent = sliding_window_count_within(&g->rate_window, current_time, window_us);
    rate = (float)recent / g->config.update_window_sec;
    printf("Current update rate: %.1f/sec (limit: %u/sec)\n",
           rate, g->config.max_updates_per_second);
    printf("===================================\n");
}
