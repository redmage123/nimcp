/**
 * @file nimcp_capacity_manager.c
 * @brief Dynamic capacity management implementation
 * @date 2026-01-18
 *
 * Implements generic capacity management with auto-expansion, shrinking,
 * and health agent integration.
 *
 * Part of Phase 5.8: Dynamic Capacity Management
 */

#include "utils/fault_tolerance/nimcp_capacity_manager.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(capacity_manager)

/* ============================================================================
 * Logging Tag
 * ============================================================================ */

#define LOG_TAG "CapacityMgr"

/* ============================================================================
 * Configuration Defaults
 * ============================================================================ */

void capacity_config_default(capacity_config_t* config) {
    if (!config) return;

    config->initial_capacity = 512;
    config->max_capacity = 0;           /* Unlimited */
    config->growth_factor = 2.0f;
    config->shrink_threshold = 0.25f;
    config->warning_threshold = 0.9f;
    config->elevated_threshold = 0.75f;
    config->enable_auto_expand = true;
    config->enable_auto_shrink = false;
    config->enable_immune_cleanup = true;
    config->enable_trend_analysis = true;
}

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static float calculate_utilization(uint32_t count, uint32_t capacity) {
    if (capacity == 0) return 0.0f;
    return (float)count / (float)capacity;
}

static capacity_level_t calculate_level(float utilization,
                                         const capacity_config_t* config) {
    if (utilization >= 1.0f) {
        return CAPACITY_LEVEL_CRITICAL;
    } else if (utilization >= config->warning_threshold) {
        return CAPACITY_LEVEL_WARNING;
    } else if (utilization >= config->elevated_threshold) {
        return CAPACITY_LEVEL_ELEVATED;
    }
    return CAPACITY_LEVEL_NORMAL;
}

static void update_trend(capacity_manager_t* cm) {
    if (!cm || !cm->config.enable_trend_analysis) return;

    uint64_t now = nimcp_time_now_us();
    uint32_t count = atomic_load(&cm->current_count);

    cm->trend_samples[cm->trend_index] = count;
    cm->trend_times[cm->trend_index] = now;
    cm->trend_index = (cm->trend_index + 1) % 8;

    if (cm->trend_index == 0) {
        cm->trend_filled = true;
    }
}

static float calculate_growth_rate(const capacity_manager_t* cm) {
    if (!cm || !cm->trend_filled) return 0.0f;

    /* Use oldest and newest samples */
    uint8_t oldest_idx = cm->trend_index;  /* Next to be overwritten = oldest */
    uint8_t newest_idx = (cm->trend_index + 7) % 8;

    uint64_t time_diff = cm->trend_times[newest_idx] - cm->trend_times[oldest_idx];
    if (time_diff == 0) return 0.0f;

    int32_t count_diff = (int32_t)cm->trend_samples[newest_idx] -
                         (int32_t)cm->trend_samples[oldest_idx];

    /* Convert to items per second */
    float seconds = (float)time_diff / 1000000.0f;
    return (float)count_diff / seconds;
}

static void update_peak(capacity_manager_t* cm) {
    if (!cm) return;

    uint32_t count = atomic_load(&cm->current_count);
    uint32_t capacity = atomic_load(&cm->capacity);
    float util = calculate_utilization(count, capacity);

    /* Atomically update peaks if current is higher */
    float peak_util;
    do {
        peak_util = atomic_load(&cm->peak_utilization);
        if (util <= peak_util) break;
    } while (!atomic_compare_exchange_weak(&cm->peak_utilization, &peak_util, util));

    uint32_t peak_count;
    do {
        peak_count = atomic_load(&cm->peak_count);
        if (count <= peak_count) break;
    } while (!atomic_compare_exchange_weak(&cm->peak_count, &peak_count, count));
}

/* ============================================================================
 * Capacity Manager Lifecycle
 * ============================================================================ */

int capacity_manager_create(capacity_manager_t** cm,
                            const capacity_config_t* config,
                            const char* module_name) {
    if (!cm) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cm is NULL");

        return -1;

    }

    *cm = (capacity_manager_t*)nimcp_calloc(1, sizeof(capacity_manager_t));
    if (!*cm) {
        LOG_ERROR("Failed to allocate capacity manager");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "update_peak: validation failed");
        return -1;
    }

    if (capacity_manager_init(*cm, config, module_name) != 0) {
        nimcp_free(*cm);
        *cm = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "update_peak: validation failed");
        return -1;
    }

    return 0;
}

int capacity_manager_init(capacity_manager_t* cm,
                          const capacity_config_t* config,
                          const char* module_name) {
    if (!cm) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cm is NULL");

        return -1;

    }

    memset(cm, 0, sizeof(*cm));

    /* Apply configuration */
    if (config) {
        cm->config = *config;
    } else {
        capacity_config_default(&cm->config);
    }

    /* Initialize state */
    atomic_store(&cm->current_count, 0);
    atomic_store(&cm->capacity, cm->config.initial_capacity);
    atomic_store(&cm->peak_utilization, 0.0f);

    /* Set module name */
    if (module_name) {
        strncpy(cm->module_name, module_name, sizeof(cm->module_name) - 1);
        cm->module_name[sizeof(cm->module_name) - 1] = '\0';
    } else {
        strncpy(cm->module_name, "unknown", sizeof(cm->module_name) - 1);
    }

    /* Initialize timing */
    atomic_store(&cm->last_check_time, nimcp_time_now_us());

    /* Set magic */
    cm->magic = CAPACITY_MANAGER_MAGIC;

    LOG_DEBUG("Capacity manager '%s' initialized: initial=%u, max=%u",
              cm->module_name, cm->config.initial_capacity, cm->config.max_capacity);

    return 0;
}

void capacity_manager_destroy(capacity_manager_t* cm) {
    if (!cm) return;

    if (cm->magic != CAPACITY_MANAGER_MAGIC) {
        LOG_WARN("Destroying invalid capacity manager");
        return;
    }

    cm->magic = 0;  /* Invalidate */
    nimcp_free(cm);
}

int capacity_manager_set_callbacks(capacity_manager_t* cm,
                                   void* module,
                                   capacity_expand_callback_t expand,
                                   capacity_shrink_callback_t shrink,
                                   capacity_cleanup_callback_t cleanup) {
    if (!cm || cm->magic != CAPACITY_MANAGER_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "capacity_manager_destroy: cm is NULL");
        return -1;
    }

    cm->module = module;
    cm->expand_callback = expand;
    cm->shrink_callback = shrink;
    cm->cleanup_callback = cleanup;

    return 0;
}

/* ============================================================================
 * Slot Management
 * ============================================================================ */

int capacity_manager_request_slot(capacity_manager_t* cm) {
    if (!cm || cm->magic != CAPACITY_MANAGER_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "capacity_manager_request_slot: cm is NULL");
        return -1;
    }

    uint32_t count = atomic_load(&cm->current_count);
    uint32_t cap = atomic_load(&cm->capacity);

    /* Check if at capacity */
    if (count >= cap) {
        /* Try expansion first if enabled */
        if (cm->config.enable_auto_expand) {
            if (capacity_manager_trigger_expand(cm) == 0) {
                /* Expansion succeeded, retry */
                cap = atomic_load(&cm->capacity);
            } else if (cm->config.enable_immune_cleanup && cm->cleanup_callback) {
                /* Try cleanup */
                uint32_t target = cap / 5;  /* Free ~20% */
                int freed = cm->cleanup_callback(cm->module, target);
                if (freed > 0) {
                    atomic_fetch_add(&cm->cleanup_triggers, 1);
                    count = atomic_load(&cm->current_count);
                } else {
                    atomic_fetch_add(&cm->failed_allocations, 1);
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "capacity_manager_request_slot: validation failed");
                    return -1;
                }
            } else {
                atomic_fetch_add(&cm->failed_allocations, 1);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "capacity_manager_request_slot: validation failed");
                return -1;
            }
        } else {
            atomic_fetch_add(&cm->failed_allocations, 1);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "capacity_manager_request_slot: operation failed");
            return -1;
        }
    }

    /* Increment count */
    atomic_fetch_add(&cm->current_count, 1);
    update_peak(cm);
    update_trend(cm);

    return 0;
}

int capacity_manager_release_slot(capacity_manager_t* cm) {
    if (!cm || cm->magic != CAPACITY_MANAGER_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "capacity_manager_release_slot: cm is NULL");
        return -1;
    }

    uint32_t count = atomic_load(&cm->current_count);
    if (count == 0) {
        LOG_WARN("Capacity manager '%s': release called with count=0", cm->module_name);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "capacity_manager_release_slot: count is zero");
        return -1;
    }

    atomic_fetch_sub(&cm->current_count, 1);
    update_trend(cm);

    /* Check for auto-shrink */
    if (cm->config.enable_auto_shrink) {
        count = atomic_load(&cm->current_count);
        uint32_t cap = atomic_load(&cm->capacity);
        float util = calculate_utilization(count, cap);

        if (util < cm->config.shrink_threshold &&
            cap > cm->config.initial_capacity) {
            capacity_manager_trigger_shrink(cm);
        }
    }

    return 0;
}

/* ============================================================================
 * Check and Status
 * ============================================================================ */

capacity_level_t capacity_manager_check(capacity_manager_t* cm, uint32_t current_count) {
    if (!cm || cm->magic != CAPACITY_MANAGER_MAGIC) {
        return CAPACITY_LEVEL_EXCEEDED;
    }

    /* Update count if provided externally */
    if (current_count != atomic_load(&cm->current_count)) {
        capacity_manager_set_count(cm, current_count);
    }

    uint32_t cap = atomic_load(&cm->capacity);
    float util = calculate_utilization(current_count, cap);
    capacity_level_t level = calculate_level(util, &cm->config);

    /* Take action based on level */
    if (level == CAPACITY_LEVEL_CRITICAL && cm->config.enable_auto_expand) {
        if (capacity_manager_trigger_expand(cm) == 0) {
            cap = atomic_load(&cm->capacity);
            util = calculate_utilization(current_count, cap);
            level = calculate_level(util, &cm->config);
        }
    }

    atomic_store(&cm->last_check_time, nimcp_time_now_us());

    return level;
}

void capacity_manager_set_count(capacity_manager_t* cm, uint32_t count) {
    if (!cm || cm->magic != CAPACITY_MANAGER_MAGIC) return;

    atomic_store(&cm->current_count, count);
    update_peak(cm);
    update_trend(cm);
}

float capacity_manager_get_utilization(const capacity_manager_t* cm) {
    if (!cm || cm->magic != CAPACITY_MANAGER_MAGIC) return 0.0f;

    uint32_t count = atomic_load(&cm->current_count);
    uint32_t cap = atomic_load(&cm->capacity);
    return calculate_utilization(count, cap);
}

capacity_level_t capacity_manager_get_level(const capacity_manager_t* cm) {
    if (!cm || cm->magic != CAPACITY_MANAGER_MAGIC) {
        return CAPACITY_LEVEL_EXCEEDED;
    }

    float util = capacity_manager_get_utilization(cm);
    return calculate_level(util, &cm->config);
}

bool capacity_manager_is_full(const capacity_manager_t* cm) {
    if (!cm || cm->magic != CAPACITY_MANAGER_MAGIC) return true;

    uint32_t count = atomic_load(&cm->current_count);
    uint32_t cap = atomic_load(&cm->capacity);
    return count >= cap;
}

/* ============================================================================
 * Capacity Operations
 * ============================================================================ */

int capacity_manager_trigger_expand(capacity_manager_t* cm) {
    if (!cm || cm->magic != CAPACITY_MANAGER_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "capacity_manager_trigger_expand: cm is NULL");
        return -1;
    }

    uint32_t cap = atomic_load(&cm->capacity);
    uint32_t new_cap = (uint32_t)((float)cap * cm->config.growth_factor);

    /* Ensure we grow by at least 1 */
    if (new_cap <= cap) new_cap = cap + 1;

    /* Check max limit */
    if (cm->config.max_capacity > 0 && new_cap > cm->config.max_capacity) {
        if (cap >= cm->config.max_capacity) {
            LOG_WARN("Capacity manager '%s': at max capacity %u",
                     cm->module_name, cm->config.max_capacity);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "capacity_manager_trigger_expand: capacity exceeded");
            return -1;
        }
        new_cap = cm->config.max_capacity;
    }

    /* Call expansion callback if available */
    if (cm->expand_callback) {
        if (cm->expand_callback(cm->module, new_cap) != 0) {
            LOG_ERROR("Capacity manager '%s': expansion callback failed", cm->module_name);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "capacity_manager_trigger_expand: validation failed");
            return -1;
        }
    }

    atomic_store(&cm->capacity, new_cap);
    atomic_fetch_add(&cm->expansions, 1);
    atomic_store(&cm->last_expansion_time, nimcp_time_now_us());

    LOG_INFO("Capacity manager '%s': expanded %u -> %u", cm->module_name, cap, new_cap);

    return 0;
}

int capacity_manager_trigger_shrink(capacity_manager_t* cm) {
    if (!cm || cm->magic != CAPACITY_MANAGER_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "capacity_manager_trigger_shrink: cm is NULL");
        return -1;
    }

    uint32_t cap = atomic_load(&cm->capacity);
    uint32_t count = atomic_load(&cm->current_count);

    /* Calculate new capacity - at least current count, at least initial */
    uint32_t new_cap = (uint32_t)((float)cap / cm->config.growth_factor);
    if (new_cap < count) new_cap = count;
    if (new_cap < cm->config.initial_capacity) new_cap = cm->config.initial_capacity;

    if (new_cap >= cap) {
        /* No shrink needed */
        return 0;
    }

    /* Call shrink callback if available */
    if (cm->shrink_callback) {
        if (cm->shrink_callback(cm->module, new_cap) != 0) {
            LOG_WARN("Capacity manager '%s': shrink callback failed", cm->module_name);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "capacity_manager_trigger_shrink: validation failed");
            return -1;
        }
    }

    atomic_store(&cm->capacity, new_cap);
    atomic_fetch_add(&cm->shrinks, 1);

    LOG_INFO("Capacity manager '%s': shrunk %u -> %u", cm->module_name, cap, new_cap);

    return 0;
}

int capacity_manager_trigger_cleanup(capacity_manager_t* cm, uint32_t target_free) {
    if (!cm || cm->magic != CAPACITY_MANAGER_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "capacity_manager_trigger_cleanup: cm is NULL");
        return -1;
    }

    if (!cm->cleanup_callback) {
        LOG_DEBUG("Capacity manager '%s': no cleanup callback", cm->module_name);
        return 0;
    }

    int freed = cm->cleanup_callback(cm->module, target_free);
    if (freed > 0) {
        atomic_fetch_add(&cm->cleanup_triggers, 1);
        LOG_INFO("Capacity manager '%s': cleanup freed %d slots", cm->module_name, freed);
    }

    return freed;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

void capacity_manager_get_stats(const capacity_manager_t* cm, capacity_stats_t* stats) {
    if (!stats) return;
    memset(stats, 0, sizeof(*stats));

    if (!cm || cm->magic != CAPACITY_MANAGER_MAGIC) return;

    stats->current_count = atomic_load(&cm->current_count);
    stats->capacity = atomic_load(&cm->capacity);
    stats->utilization = calculate_utilization(stats->current_count, stats->capacity);
    stats->level = calculate_level(stats->utilization, &cm->config);

    stats->expansions = atomic_load(&cm->expansions);
    stats->shrinks = atomic_load(&cm->shrinks);
    stats->cleanup_triggers = atomic_load(&cm->cleanup_triggers);
    stats->failed_allocations = atomic_load(&cm->failed_allocations);
    stats->peak_utilization = atomic_load(&cm->peak_utilization);
    stats->peak_count = atomic_load(&cm->peak_count);

    stats->growth_rate_per_sec = calculate_growth_rate(cm);
    if (stats->growth_rate_per_sec > 0) {
        uint32_t remaining = stats->capacity - stats->current_count;
        stats->time_to_capacity_sec = (float)remaining / stats->growth_rate_per_sec;
        stats->growth_trend_valid = cm->trend_filled;
    } else {
        stats->time_to_capacity_sec = -1.0f;
        stats->growth_trend_valid = false;
    }

    stats->last_expansion_time = atomic_load(&cm->last_expansion_time);
    stats->last_check_time = atomic_load(&cm->last_check_time);
}

float capacity_manager_time_to_capacity(const capacity_manager_t* cm) {
    if (!cm || cm->magic != CAPACITY_MANAGER_MAGIC) return -1.0f;
    if (!cm->trend_filled) return -1.0f;

    float rate = calculate_growth_rate(cm);
    if (rate <= 0) return -1.0f;

    uint32_t count = atomic_load(&cm->current_count);
    uint32_t cap = atomic_load(&cm->capacity);
    if (count >= cap) return 0.0f;

    return (float)(cap - count) / rate;
}

void capacity_manager_reset_stats(capacity_manager_t* cm) {
    if (!cm || cm->magic != CAPACITY_MANAGER_MAGIC) return;

    atomic_store(&cm->expansions, 0);
    atomic_store(&cm->shrinks, 0);
    atomic_store(&cm->cleanup_triggers, 0);
    atomic_store(&cm->failed_allocations, 0);
    atomic_store(&cm->peak_utilization, 0.0f);
    atomic_store(&cm->peak_count, 0);

    memset(cm->trend_samples, 0, sizeof(cm->trend_samples));
    memset(cm->trend_times, 0, sizeof(cm->trend_times));
    cm->trend_index = 0;
    cm->trend_filled = false;
}
