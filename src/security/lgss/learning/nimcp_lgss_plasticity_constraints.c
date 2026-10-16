/**
 * @file nimcp_lgss_plasticity_constraints.c
 * @brief Implementation of LGSS Plasticity Constraints Guard
 * @version 1.0.0
 * @date 2026-01-16
 *
 * Implements the plasticity guard that constrains all synaptic weight updates
 * to prevent modification of safety-critical synapses and ensure biologically
 * plausible learning.
 */

#include "security/lgss/learning/nimcp_lgss_plasticity_constraints.h"
#include "utils/validation/nimcp_common.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(lgss_plasticity_constraints, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

/** Frozen synapse hashmap entry */
typedef struct frozen_entry {
    uint64_t synapse_id;
    bool occupied;
} frozen_entry_t;

/** Sliding window for rate limiting */
typedef struct {
    uint64_t* timestamps;       /**< Circular buffer of update timestamps */
    uint32_t capacity;          /**< Buffer capacity */
    uint32_t head;              /**< Write position */
    uint32_t count;             /**< Current count */
} sliding_window_t;

/** Internal plasticity guard structure */
struct plasticity_guard_internal {
    uint32_t magic;                         /**< Magic number for validation */

    /* Configuration */
    plasticity_safety_config_t config;

    /* Frozen synapse hashmap */
    frozen_entry_t* frozen_synapses;        /**< Hashmap of frozen synapses */
    uint32_t frozen_hashmap_size;           /**< Hashmap size (power of 2) */
    uint32_t frozen_count;                  /**< Number of frozen synapses */

    /* Frozen pathways */
    uint32_t* frozen_pathways;              /**< Array of frozen pathway IDs */
    uint32_t frozen_pathway_count;          /**< Number of frozen pathways */
    uint32_t frozen_pathway_capacity;       /**< Capacity of pathway array */

    /* Reward pathway tracking */
    frozen_entry_t* reward_synapses;        /**< Hashmap of reward synapses */
    uint32_t reward_hashmap_size;           /**< Reward hashmap size */
    uint32_t reward_synapse_count;          /**< Number of reward synapses */

    /* Self-reward tracking */
    frozen_entry_t* self_reward_synapses;   /**< Hashmap of self-reward synapses */
    uint32_t self_reward_hashmap_size;      /**< Self-reward hashmap size */
    uint32_t self_reward_count;             /**< Number of self-reward synapses */

    /* Rate limiting */
    sliding_window_t rate_window;           /**< Sliding window for rate limiting */

    /* Weight drift tracking */
    float cumulative_drift;                 /**< Cumulative weight drift */

    /* Homeostatic state */
    float current_activity;                 /**< Current network activity */
    float activity_ewma;                    /**< EWMA of activity */

    /* Statistics */
    plasticity_guard_stats_t stats;

    /* Security orchestrator */
    security_orchestrator_t orchestrator;

    /* Thread synchronization (placeholder - would use mutex in production) */
    bool locked;
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
 * Hash function for synapse IDs
 */
static uint32_t hash_synapse_id(uint64_t id, uint32_t size) {
    /* Simple multiplicative hash */
    uint64_t hash = id * 0x9E3779B97F4A7C15ULL;
    return (uint32_t)(hash % size);
}

/**
 * Initialize a hashmap for synapse tracking
 */
static frozen_entry_t* hashmap_create(uint32_t size) {
    frozen_entry_t* map = nimcp_calloc(size, sizeof(frozen_entry_t));
    return map;
}

/**
 * Insert into hashmap with linear probing
 */
static bool hashmap_insert(frozen_entry_t* map, uint32_t size,
                           uint64_t synapse_id, uint32_t* count) {
    uint32_t idx = hash_synapse_id(synapse_id, size);
    uint32_t start = idx;

    do {
        if (!map[idx].occupied) {
            map[idx].synapse_id = synapse_id;
            map[idx].occupied = true;
            (*count)++;
            return true;
        }
        if (map[idx].synapse_id == synapse_id) {
            /* Already exists */
            return false;
        }
        idx = (idx + 1) % size;
    } while (idx != start);

    /* Hashmap full */
    return false;
}

/**
 * Lookup in hashmap
 */
static bool hashmap_contains(frozen_entry_t* map, uint32_t size, uint64_t synapse_id) {
    if (!map) {
        return false;
    }

    uint32_t idx = hash_synapse_id(synapse_id, size);
    uint32_t start = idx;

    do {
        if (!map[idx].occupied) {
            return false;
        }
        if (map[idx].synapse_id == synapse_id) {
            return true;
        }
        idx = (idx + 1) % size;
    } while (idx != start);

    return false;
}

/**
 * Remove from hashmap
 */
static bool hashmap_remove(frozen_entry_t* map, uint32_t size,
                           uint64_t synapse_id, uint32_t* count) {
    if (!map) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hashmap_remove: map is NULL");
        return false;
    }

    uint32_t idx = hash_synapse_id(synapse_id, size);
    uint32_t start = idx;

    do {
        if (!map[idx].occupied) {
            return false;
        }
        if (map[idx].synapse_id == synapse_id) {
            map[idx].occupied = false;
            (*count)--;
            return true;
        }
        idx = (idx + 1) % size;
    } while (idx != start);

    return false;
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

/* ============================================================================
 * PUBLIC API IMPLEMENTATION
 * ============================================================================ */

plasticity_safety_config_t plasticity_default_config(void) {
    plasticity_safety_config_t config = {
        .max_weight_change_per_update = LGSS_DEFAULT_MAX_WEIGHT_CHANGE,
        .max_learning_rate = LGSS_DEFAULT_MAX_LEARNING_RATE,
        .min_learning_rate = LGSS_DEFAULT_MIN_LEARNING_RATE,
        .max_updates_per_second = LGSS_DEFAULT_MAX_UPDATES_PER_SEC,
        .rate_limit_window_sec = LGSS_RATE_LIMIT_WINDOW_SEC,
        .block_self_reward = true,
        .block_reward_pathway_mod = true,
        .frozen_synapse_ids = NULL,
        .num_frozen_synapses = 0,
        .max_total_weight_drift = LGSS_DEFAULT_MAX_TOTAL_DRIFT,
        .homeostatic_target = LGSS_DEFAULT_HOMEOSTATIC_TARGET,
        .enable_homeostatic_regulation = true,
        .homeostatic_time_constant = 10.0f,
        .min_weight = -1.0f,
        .max_weight = 1.0f,
        .enable_violation_logging = true,
        .enable_statistics = true
    };
    return config;
}

plasticity_guard_t plasticity_guard_create(
    const plasticity_safety_config_t* config,
    security_orchestrator_t orchestrator
) {
    struct plasticity_guard_internal* guard = nimcp_calloc(1, sizeof(*guard));
    if (!guard) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "guard is NULL");

        return NULL;
    }

    guard->magic = LGSS_PLASTICITY_GUARD_MAGIC;

    /* Copy configuration */
    if (config) {
        guard->config = *config;
    } else {
        guard->config = plasticity_default_config();
    }

    /* Initialize frozen synapse hashmap */
    guard->frozen_hashmap_size = LGSS_MAX_FROZEN_SYNAPSES * 2; /* Load factor ~0.5 */
    guard->frozen_synapses = hashmap_create(guard->frozen_hashmap_size);
    if (!guard->frozen_synapses) {
        nimcp_free(guard);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "plasticity_guard_create: guard->frozen_synapses is NULL");
        return NULL;
    }

    /* Add any pre-configured frozen synapses */
    if (guard->config.frozen_synapse_ids && guard->config.num_frozen_synapses > 0) {
        for (uint32_t i = 0; i < guard->config.num_frozen_synapses; i++) {
            hashmap_insert(guard->frozen_synapses, guard->frozen_hashmap_size,
                          guard->config.frozen_synapse_ids[i], &guard->frozen_count);
        }
    }

    /* Initialize frozen pathways array */
    guard->frozen_pathway_capacity = LGSS_MAX_FROZEN_PATHWAYS;
    guard->frozen_pathways = nimcp_calloc(guard->frozen_pathway_capacity, sizeof(uint32_t));
    if (!guard->frozen_pathways) {
        nimcp_free(guard->frozen_synapses);
        nimcp_free(guard);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "plasticity_guard_create: guard->frozen_pathways is NULL");
        return NULL;
    }

    /* Initialize reward synapse tracking */
    guard->reward_hashmap_size = 1024;
    guard->reward_synapses = hashmap_create(guard->reward_hashmap_size);

    /* Initialize self-reward synapse tracking */
    guard->self_reward_hashmap_size = 256;
    guard->self_reward_synapses = hashmap_create(guard->self_reward_hashmap_size);

    /* Initialize rate limiting sliding window */
    uint32_t window_capacity = (uint32_t)(guard->config.max_updates_per_second *
                                          guard->config.rate_limit_window_sec * 2);
    if (window_capacity < 100) window_capacity = 100;
    if (!sliding_window_init(&guard->rate_window, window_capacity)) {
        nimcp_free(guard->self_reward_synapses);
        nimcp_free(guard->reward_synapses);
        nimcp_free(guard->frozen_pathways);
        nimcp_free(guard->frozen_synapses);
        nimcp_free(guard);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "plasticity_guard_create: sliding_window_init is NULL");
        return NULL;
    }

    /* Initialize homeostatic state */
    guard->current_activity = guard->config.homeostatic_target;
    guard->activity_ewma = guard->config.homeostatic_target;

    /* Store orchestrator reference */
    guard->orchestrator = orchestrator;

    return guard;
}

void plasticity_guard_destroy(plasticity_guard_t guard) {
    if (!guard) return;

    struct plasticity_guard_internal* g = guard;
    if (g->magic != LGSS_PLASTICITY_GUARD_MAGIC) return;

    sliding_window_destroy(&g->rate_window);
    nimcp_free(g->self_reward_synapses);
    nimcp_free(g->reward_synapses);
    nimcp_free(g->frozen_pathways);
    nimcp_free(g->frozen_synapses);

    g->magic = 0;
    nimcp_free(g);
}

int plasticity_guard_reset(plasticity_guard_t guard) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");

    struct plasticity_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_PLASTICITY_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid guard magic");

    /* Reset rate window */
    g->rate_window.head = 0;
    g->rate_window.count = 0;

    /* Reset drift tracking */
    g->cumulative_drift = 0.0f;

    /* Reset homeostatic state */
    g->current_activity = g->config.homeostatic_target;
    g->activity_ewma = g->config.homeostatic_target;

    /* Reset statistics */
    memset(&g->stats, 0, sizeof(g->stats));

    return NIMCP_SUCCESS;
}

int plasticity_guard_freeze_synapse(plasticity_guard_t guard, uint64_t synapse_id) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");

    struct plasticity_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_PLASTICITY_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid guard magic");
    NIMCP_CHECK_THROW(g->frozen_count < LGSS_MAX_FROZEN_SYNAPSES, NIMCP_ERROR_OUT_OF_RANGE, "frozen synapse limit exceeded");

    if (!hashmap_insert(g->frozen_synapses, g->frozen_hashmap_size,
                       synapse_id, &g->frozen_count)) {
        return NIMCP_ERROR_ALREADY_EXISTS;
    }

    return NIMCP_SUCCESS;
}

int plasticity_guard_unfreeze_synapse(plasticity_guard_t guard, uint64_t synapse_id) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");

    struct plasticity_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_PLASTICITY_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid guard magic");

    if (!hashmap_remove(g->frozen_synapses, g->frozen_hashmap_size,
                       synapse_id, &g->frozen_count)) {
        return NIMCP_ERROR_NOT_FOUND;
    }

    return NIMCP_SUCCESS;
}

bool plasticity_guard_is_frozen(plasticity_guard_t guard, uint64_t synapse_id) {
    if (!guard) { return false; }

    struct plasticity_guard_internal* g = guard;
    if (g->magic != LGSS_PLASTICITY_GUARD_MAGIC) { return false; }

    return hashmap_contains(g->frozen_synapses, g->frozen_hashmap_size, synapse_id);
}

int plasticity_guard_freeze_pathway(plasticity_guard_t guard, uint32_t pathway_id) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");

    struct plasticity_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_PLASTICITY_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid guard magic");
    NIMCP_CHECK_THROW(g->frozen_pathway_count < g->frozen_pathway_capacity, NIMCP_ERROR_OUT_OF_RANGE, "frozen pathway limit exceeded");

    /* Check for duplicates */
    for (uint32_t i = 0; i < g->frozen_pathway_count; i++) {
        if (g->frozen_pathways[i] == pathway_id) {
            return NIMCP_ERROR_ALREADY_EXISTS;
        }
    }

    g->frozen_pathways[g->frozen_pathway_count++] = pathway_id;
    return NIMCP_SUCCESS;
}

int plasticity_guard_unfreeze_pathway(plasticity_guard_t guard, uint32_t pathway_id) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");

    struct plasticity_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_PLASTICITY_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid guard magic");

    for (uint32_t i = 0; i < g->frozen_pathway_count; i++) {
        if (g->frozen_pathways[i] == pathway_id) {
            /* Shift remaining elements */
            for (uint32_t j = i; j < g->frozen_pathway_count - 1; j++) {
                g->frozen_pathways[j] = g->frozen_pathways[j + 1];
            }
            g->frozen_pathway_count--;
            return NIMCP_SUCCESS;
        }
    }

    return NIMCP_ERROR_NOT_FOUND;
}

uint32_t plasticity_guard_freeze_bulk(
    plasticity_guard_t guard,
    const uint64_t* synapse_ids,
    uint32_t count
) {
    if (!guard || !synapse_ids || count == 0) return 0;

    uint32_t frozen = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (plasticity_guard_freeze_synapse(guard, synapse_ids[i]) == NIMCP_SUCCESS) {
            frozen++;
        }
    }
    return frozen;
}

bool plasticity_guard_would_violate(
    plasticity_guard_t guard,
    uint64_t synapse_id,
    float old_weight,
    float new_weight
) {
    plasticity_check_result_t result;
    if (plasticity_guard_check_update(guard, synapse_id, old_weight, new_weight, &result) != 0) {
        return true;
    }
    return !result.allowed;
}

int plasticity_guard_check_update(
    plasticity_guard_t guard,
    uint64_t synapse_id,
    float old_weight,
    float new_weight,
    plasticity_check_result_t* result
) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_NULL_POINTER, "result is NULL");

    struct plasticity_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_PLASTICITY_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid guard magic");

    /* Initialize result */
    memset(result, 0, sizeof(*result));
    result->allowed = true;
    result->original_delta = new_weight - old_weight;
    result->adjusted_delta = result->original_delta;
    result->adjusted_weight = new_weight;

    /* Check frozen synapse */
    if (hashmap_contains(g->frozen_synapses, g->frozen_hashmap_size, synapse_id)) {
        result->violations |= PLASTICITY_VIOLATION_FROZEN_SYNAPSE;
        result->allowed = false;
        snprintf(result->reason, sizeof(result->reason),
                "Synapse %lu is frozen", (unsigned long)synapse_id);
        return NIMCP_SUCCESS;
    }

    /* Check self-reward synapse */
    if (g->config.block_self_reward &&
        hashmap_contains(g->self_reward_synapses, g->self_reward_hashmap_size, synapse_id)) {
        result->violations |= PLASTICITY_VIOLATION_SELF_REWARD;
        result->allowed = false;
        snprintf(result->reason, sizeof(result->reason),
                "Self-reward synapse modification blocked");
        return NIMCP_SUCCESS;
    }

    /* Check reward pathway synapse */
    if (g->config.block_reward_pathway_mod &&
        hashmap_contains(g->reward_synapses, g->reward_hashmap_size, synapse_id)) {
        result->violations |= PLASTICITY_VIOLATION_REWARD_PATHWAY;
        result->allowed = false;
        snprintf(result->reason, sizeof(result->reason),
                "Reward pathway modification blocked");
        return NIMCP_SUCCESS;
    }

    /* Check weight bounds */
    if (new_weight < g->config.min_weight || new_weight > g->config.max_weight) {
        result->violations |= PLASTICITY_VIOLATION_INVALID_WEIGHT;
        /* Clamp the weight */
        if (new_weight < g->config.min_weight) {
            result->adjusted_weight = g->config.min_weight;
        } else {
            result->adjusted_weight = g->config.max_weight;
        }
        result->adjusted_delta = result->adjusted_weight - old_weight;
    }

    /* Check magnitude */
    float delta = fabsf(result->adjusted_delta);
    if (delta > g->config.max_weight_change_per_update) {
        result->violations |= PLASTICITY_VIOLATION_MAGNITUDE;
        /* Clamp the delta */
        float sign = result->adjusted_delta >= 0 ? 1.0f : -1.0f;
        result->adjusted_delta = sign * g->config.max_weight_change_per_update;
        result->adjusted_weight = old_weight + result->adjusted_delta;
    }

    /* Check total drift */
    float new_drift = g->cumulative_drift + fabsf(result->adjusted_delta);
    if (new_drift > g->config.max_total_weight_drift) {
        result->violations |= PLASTICITY_VIOLATION_TOTAL_DRIFT;
        result->allowed = false;
        snprintf(result->reason, sizeof(result->reason),
                "Total weight drift limit exceeded (%.3f > %.3f)",
                new_drift, g->config.max_total_weight_drift);
        return NIMCP_SUCCESS;
    }

    /* Check homeostatic constraint */
    if (g->config.enable_homeostatic_regulation) {
        float deviation = fabsf(g->activity_ewma - g->config.homeostatic_target);
        if (deviation > 0.3f) {
            /* Activity is far from target - check if update moves us further */
            float activity_change_direction = (result->adjusted_delta > 0) ? 1.0f : -1.0f;
            float current_direction = (g->activity_ewma > g->config.homeostatic_target) ? 1.0f : -1.0f;

            if (activity_change_direction == current_direction) {
                /* Update would increase deviation */
                result->violations |= PLASTICITY_VIOLATION_HOMEOSTATIC;
                /* Allow but flag the violation */
            }
        }
    }

    /* Note: Rate limiting is only checked in apply_update, not in check_update */

    return NIMCP_SUCCESS;
}

int plasticity_guard_apply_update(
    plasticity_guard_t guard,
    uint64_t synapse_id,
    float* weight_ptr
) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");
    NIMCP_CHECK_THROW(weight_ptr, NIMCP_ERROR_NULL_POINTER, "weight_ptr is NULL");

    struct plasticity_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_PLASTICITY_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid guard magic");

    uint64_t start_time = get_time_us();

    /* Update statistics */
    g->stats.total_updates_attempted++;

    /* Check rate limiting first */
    uint64_t current_time = get_time_us();
    uint64_t window_us = (uint64_t)(g->config.rate_limit_window_sec * 1000000.0f);
    uint32_t recent_updates = sliding_window_count_within(&g->rate_window, current_time, window_us);

    if (recent_updates >= g->config.max_updates_per_second) {
        g->stats.rate_limit_violations++;
        g->stats.updates_blocked++;
        return PLASTICITY_VIOLATION_RATE_LIMIT;
    }

    /* Perform the check */
    float old_weight = *weight_ptr;
    plasticity_check_result_t result;
    int err = plasticity_guard_check_update(guard, synapse_id, old_weight, old_weight, &result);
    if (err != NIMCP_SUCCESS) {
        g->stats.updates_blocked++;
        return err;
    }

    /* Check the actual update */
    float new_weight = *weight_ptr;
    err = plasticity_guard_check_update(guard, synapse_id, old_weight, new_weight, &result);
    if (err != NIMCP_SUCCESS) {
        g->stats.updates_blocked++;
        return err;
    }

    if (!result.allowed) {
        g->stats.updates_blocked++;
        /* Update violation statistics */
        if (result.violations & PLASTICITY_VIOLATION_FROZEN_SYNAPSE) {
            g->stats.frozen_synapse_violations++;
        }
        if (result.violations & PLASTICITY_VIOLATION_SELF_REWARD) {
            g->stats.self_reward_violations++;
        }
        if (result.violations & PLASTICITY_VIOLATION_REWARD_PATHWAY) {
            g->stats.reward_pathway_violations++;
        }
        if (result.violations & PLASTICITY_VIOLATION_TOTAL_DRIFT) {
            g->stats.total_drift_violations++;
        }
        return (int)result.violations;
    }

    /* Apply the (possibly clamped) update */
    *weight_ptr = result.adjusted_weight;

    /* Update rate limiting window */
    sliding_window_add(&g->rate_window, current_time);

    /* Update drift tracking */
    g->cumulative_drift += fabsf(result.adjusted_weight - old_weight);

    /* Update statistics */
    g->stats.updates_allowed++;
    float change = fabsf(result.adjusted_weight - old_weight);
    g->stats.avg_weight_change = (g->stats.avg_weight_change * (g->stats.updates_allowed - 1) +
                                   change) / g->stats.updates_allowed;
    if (change > g->stats.max_weight_change_seen) {
        g->stats.max_weight_change_seen = change;
    }

    /* Track violation statistics for clamped updates */
    if (result.violations & PLASTICITY_VIOLATION_MAGNITUDE) {
        g->stats.magnitude_violations++;
    }
    if (result.violations & PLASTICITY_VIOLATION_HOMEOSTATIC) {
        g->stats.homeostatic_violations++;
    }
    if (result.violations & PLASTICITY_VIOLATION_INVALID_WEIGHT) {
        /* Counted implicitly via clamping */
    }

    /* Update timing statistics */
    uint64_t elapsed = get_time_us() - start_time;
    g->stats.guard_overhead_us += elapsed;
    g->stats.avg_check_time_us = (float)g->stats.guard_overhead_us /
                                  (float)g->stats.total_updates_attempted;

    return (int)result.violations; /* 0 if no violations */
}

int plasticity_guard_apply_update_with_lr(
    plasticity_guard_t guard,
    uint64_t synapse_id,
    float old_weight,
    float* weight_ptr,
    float learning_rate
) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");
    NIMCP_CHECK_THROW(weight_ptr, NIMCP_ERROR_NULL_POINTER, "weight_ptr is NULL");

    struct plasticity_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_PLASTICITY_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid guard magic");

    /* Check learning rate bounds */
    if (learning_rate < g->config.min_learning_rate ||
        learning_rate > g->config.max_learning_rate) {
        g->stats.learning_rate_violations++;
        return PLASTICITY_VIOLATION_LEARNING_RATE;
    }

    /* Apply the update */
    return plasticity_guard_apply_update(guard, synapse_id, weight_ptr);
}

uint32_t plasticity_guard_apply_batch(
    plasticity_guard_t guard,
    const uint64_t* synapse_ids,
    const float* old_weights,
    float* new_weights,
    uint32_t count,
    plasticity_violation_t* violations_out
) {
    if (!guard || !synapse_ids || !old_weights || !new_weights || count == 0) {
        return 0;
    }

    uint32_t allowed = 0;
    for (uint32_t i = 0; i < count; i++) {
        int result = plasticity_guard_apply_update(guard, synapse_ids[i], &new_weights[i]);
        if (violations_out) {
            violations_out[i] = (plasticity_violation_t)result;
        }
        if (result == 0 || (result > 0 && result < 512)) {
            /* Update was allowed (possibly with clamping) */
            allowed++;
        } else {
            /* Update was blocked - restore original weight */
            new_weights[i] = old_weights[i];
        }
    }
    return allowed;
}

int plasticity_guard_register_reward_synapse(plasticity_guard_t guard, uint64_t synapse_id) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");

    struct plasticity_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_PLASTICITY_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid guard magic");

    if (!hashmap_insert(g->reward_synapses, g->reward_hashmap_size,
                       synapse_id, &g->reward_synapse_count)) {
        return NIMCP_ERROR_ALREADY_EXISTS;
    }
    return NIMCP_SUCCESS;
}

int plasticity_guard_register_self_reward_synapse(plasticity_guard_t guard, uint64_t synapse_id) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");

    struct plasticity_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_PLASTICITY_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid guard magic");

    if (!hashmap_insert(g->self_reward_synapses, g->self_reward_hashmap_size,
                       synapse_id, &g->self_reward_count)) {
        return NIMCP_ERROR_ALREADY_EXISTS;
    }
    return NIMCP_SUCCESS;
}

int plasticity_guard_update_homeostatic_state(plasticity_guard_t guard, float current_activity) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");

    struct plasticity_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_PLASTICITY_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid guard magic");

    g->current_activity = current_activity;

    /* Update EWMA */
    float alpha = 1.0f / g->config.homeostatic_time_constant;
    g->activity_ewma = alpha * current_activity + (1.0f - alpha) * g->activity_ewma;

    return NIMCP_SUCCESS;
}

int plasticity_guard_get_homeostatic_scale(plasticity_guard_t guard, float* scale_out) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");
    NIMCP_CHECK_THROW(scale_out, NIMCP_ERROR_NULL_POINTER, "scale_out is NULL");

    struct plasticity_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_PLASTICITY_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid guard magic");

    /* Calculate scaling factor to move activity toward target */
    float error = g->config.homeostatic_target - g->activity_ewma;
    *scale_out = 1.0f + error * 0.1f; /* Simple proportional control */

    /* Clamp to reasonable range */
    if (*scale_out < 0.5f) *scale_out = 0.5f;
    if (*scale_out > 2.0f) *scale_out = 2.0f;

    return NIMCP_SUCCESS;
}

int plasticity_guard_get_stats(plasticity_guard_t guard, plasticity_guard_stats_t* stats) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");

    struct plasticity_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_PLASTICITY_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid guard magic");

    *stats = g->stats;
    stats->cumulative_drift = g->cumulative_drift;

    /* Calculate current update rate */
    uint64_t current_time = get_time_us();
    uint64_t window_us = (uint64_t)(g->config.rate_limit_window_sec * 1000000.0f);
    uint32_t recent = sliding_window_count_within(&g->rate_window, current_time, window_us);
    stats->current_update_rate = (float)recent / g->config.rate_limit_window_sec;

    return NIMCP_SUCCESS;
}

int plasticity_guard_reset_stats(plasticity_guard_t guard) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");

    struct plasticity_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_PLASTICITY_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid guard magic");

    memset(&g->stats, 0, sizeof(g->stats));
    return NIMCP_SUCCESS;
}

int plasticity_guard_get_rate_state(
    plasticity_guard_t guard,
    float* current_rate,
    uint32_t* remaining
) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");

    struct plasticity_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_PLASTICITY_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid guard magic");

    uint64_t current_time = get_time_us();
    uint64_t window_us = (uint64_t)(g->config.rate_limit_window_sec * 1000000.0f);
    uint32_t recent = sliding_window_count_within(&g->rate_window, current_time, window_us);

    if (current_rate) {
        *current_rate = (float)recent / g->config.rate_limit_window_sec;
    }
    if (remaining) {
        *remaining = (recent < g->config.max_updates_per_second) ?
                     g->config.max_updates_per_second - recent : 0;
    }

    return NIMCP_SUCCESS;
}

int plasticity_guard_get_drift(plasticity_guard_t guard, float* drift) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");
    NIMCP_CHECK_THROW(drift, NIMCP_ERROR_NULL_POINTER, "drift is NULL");

    struct plasticity_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_PLASTICITY_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid guard magic");

    *drift = g->cumulative_drift;
    return NIMCP_SUCCESS;
}

int plasticity_guard_connect_bio_async(plasticity_guard_t guard) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");
    /* Placeholder - would register with bio-async router */
    return NIMCP_SUCCESS;
}

uint32_t plasticity_guard_process_inbox(plasticity_guard_t guard, uint32_t max_messages) {
    if (!guard) return 0;
    /* Placeholder - would process bio-async messages */
    (void)max_messages;
    return 0;
}

const char* plasticity_violation_name(plasticity_violation_t violation) {
    switch (violation) {
        case PLASTICITY_VIOLATION_NONE: return "NONE";
        case PLASTICITY_VIOLATION_RATE_LIMIT: return "RATE_LIMIT";
        case PLASTICITY_VIOLATION_MAGNITUDE: return "MAGNITUDE";
        case PLASTICITY_VIOLATION_FROZEN_SYNAPSE: return "FROZEN_SYNAPSE";
        case PLASTICITY_VIOLATION_FROZEN_PATHWAY: return "FROZEN_PATHWAY";
        case PLASTICITY_VIOLATION_SELF_REWARD: return "SELF_REWARD";
        case PLASTICITY_VIOLATION_REWARD_PATHWAY: return "REWARD_PATHWAY";
        case PLASTICITY_VIOLATION_TOTAL_DRIFT: return "TOTAL_DRIFT";
        case PLASTICITY_VIOLATION_LEARNING_RATE: return "LEARNING_RATE";
        case PLASTICITY_VIOLATION_HOMEOSTATIC: return "HOMEOSTATIC";
        case PLASTICITY_VIOLATION_INVALID_WEIGHT: return "INVALID_WEIGHT";
        default: return "UNKNOWN";
    }
}

int plasticity_violations_to_string(
    plasticity_violation_t violations,
    char* buffer,
    size_t buffer_size
) {
    if (!buffer || buffer_size == 0) return 0;

    buffer[0] = '\0';
    int written = 0;

    if (violations == PLASTICITY_VIOLATION_NONE) {
        return snprintf(buffer, buffer_size, "NONE");
    }

    const char* sep = "";
    for (int i = 0; i < 10; i++) {
        plasticity_violation_t flag = (plasticity_violation_t)(1 << i);
        if (violations & flag) {
            int n = snprintf(buffer + written, buffer_size - written,
                           "%s%s", sep, plasticity_violation_name(flag));
            if (n > 0) {
                written += n;
                sep = "|";
            }
        }
    }

    return written;
}

void plasticity_guard_print_summary(plasticity_guard_t guard) {
    if (!guard) {
        printf("Plasticity Guard: NULL\n");
        return;
    }

    struct plasticity_guard_internal* g = guard;
    if (g->magic != LGSS_PLASTICITY_GUARD_MAGIC) {
        printf("Plasticity Guard: INVALID (bad magic)\n");
        return;
    }

    printf("=== Plasticity Guard Summary ===\n");
    printf("Frozen synapses: %u\n", g->frozen_count);
    printf("Frozen pathways: %u\n", g->frozen_pathway_count);
    printf("Cumulative drift: %.4f / %.4f\n",
           g->cumulative_drift, g->config.max_total_weight_drift);
    printf("Updates: %lu allowed, %lu blocked\n",
           (unsigned long)g->stats.updates_allowed,
           (unsigned long)g->stats.updates_blocked);
    printf("Homeostatic activity: %.3f (target: %.3f)\n",
           g->activity_ewma, g->config.homeostatic_target);

    float rate;
    uint32_t remaining;
    plasticity_guard_get_rate_state(guard, &rate, &remaining);
    printf("Current rate: %.1f updates/sec, %u remaining in window\n", rate, remaining);
    printf("================================\n");
}
