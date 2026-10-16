/**
 * @file nimcp_fault_working_memory.c
 * @brief Working Memory for Active Fault Context - Implementation
 * @version 1.0.0
 * @date 2025-11-20
 *
 * WHAT: Implementation of working memory for active fault tracking
 * WHY: Coordinate multi-step recovery, detect cascading failures
 * HOW: Fixed-capacity buffer with priority-based eviction
 *
 * COMPLEXITY ANALYSIS:
 * - Space: O(capacity) = O(9) for default
 * - Add: O(capacity) = O(9) for eviction search
 * - Remove: O(capacity) = O(9) for search + shift
 * - Get Priority: O(capacity) = O(9) for scan
 *
 * LOC: ~400 lines
 */

#include "cognitive/fault_tolerance/nimcp_fault_working_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

//=============================================================================
// Internal Structure Definition
//=============================================================================

/**
 * @brief Working memory internal structure
 *
 * WHAT: Hidden implementation of fault working memory
 * WHY: Encapsulation allows future changes without breaking API
 */
struct fault_working_memory {
    // Active faults array
    active_fault_t* faults;          /**< Array of active faults */
    uint32_t capacity;               /**< Maximum faults (Miller's Law: 7±2) */
    uint32_t count;                  /**< Current number of faults */

    // Recovery context
    recovery_strategy_t active_strategy;  /**< Current recovery strategy */
    uint32_t recovery_step;               /**< Current step in recovery */
    uint32_t total_steps;                 /**< Total recovery steps */

    // Cascade detection
    uint32_t cascade_threshold;      /**< Faults/min to trigger cascade */
    uint64_t cascade_window_us;      /**< Time window for cascade detection */
    uint32_t faults_in_window;       /**< Faults added in current window */
    uint64_t window_start_us;        /**< Start of current time window */
    bool cascade_detected;           /**< Whether cascade is active */

    // Priority tracking
    uint32_t priority_fault_idx;     /**< Index of highest-priority fault */
};

//=============================================================================
// Helper Functions (Internal)
//=============================================================================

/**
 * @brief Find index of lowest-priority fault for eviction
 *
 * WHAT: Scan faults to find best eviction candidate
 * WHY: Preserve important faults when at capacity
 * HOW: Priority: resolved > minor > major > critical, then oldest
 *
 * COMPLEXITY: O(n) where n = count
 *
 * @param wm Working memory instance
 * @return Index of fault to evict
 */
static uint32_t find_eviction_candidate(const fault_working_memory_t* wm) {
    if (wm->count == 0) return 0;

    uint32_t evict_idx = 0;
    fault_severity_t lowest_severity = wm->faults[0].fault.severity;
    bool found_resolved = wm->faults[0].fault.is_resolved;

    // Priority 1: Resolved faults
    // Priority 2: Lowest severity
    // Priority 3: Oldest (first in array)
    for (uint32_t i = 1; i < wm->count; i++) {
        bool is_resolved = wm->faults[i].fault.is_resolved;
        fault_severity_t severity = wm->faults[i].fault.severity;

        // Prefer resolved faults
        if (is_resolved && !found_resolved) {
            evict_idx = i;
            lowest_severity = severity;
            found_resolved = true;
        }
        // Among resolved (or all unresolved), prefer lower severity
        else if (is_resolved == found_resolved && severity < lowest_severity) {
            evict_idx = i;
            lowest_severity = severity;
        }
    }

    return evict_idx;
}

/**
 * @brief Update priority fault index
 *
 * WHAT: Find fault with highest priority
 * WHY: Maintain attention focus on most critical issue
 * HOW: Scan for highest severity, then oldest
 *
 * COMPLEXITY: O(n) where n = count
 *
 * @param wm Working memory instance
 */
static void update_priority_fault(fault_working_memory_t* wm) {
    if (wm->count == 0) {
        wm->priority_fault_idx = 0;
        return;
    }

    uint32_t priority_idx = 0;
    fault_severity_t highest_severity = wm->faults[0].fault.severity;

    // Find highest severity (critical > major > minor)
    // If tie, keep first (oldest)
    for (uint32_t i = 1; i < wm->count; i++) {
        if (wm->faults[i].fault.severity > highest_severity) {
            priority_idx = i;
            highest_severity = wm->faults[i].fault.severity;
        }
    }

    wm->priority_fault_idx = priority_idx;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

fault_working_memory_config_t fault_working_memory_default_config(void) {
    fault_working_memory_config_t config = {
        .max_capacity = FAULT_WORKING_MEMORY_DEFAULT_CAPACITY,
        .cascade_threshold = FAULT_WORKING_MEMORY_CASCADE_THRESHOLD,
        .cascade_window_us = FAULT_WORKING_MEMORY_CASCADE_WINDOW_US
    };
    return config;
}

fault_working_memory_t* fault_working_memory_create(void) {
    fault_working_memory_config_t config = fault_working_memory_default_config();
    return fault_working_memory_create_custom(&config);
}

fault_working_memory_t* fault_working_memory_create_custom(
    const fault_working_memory_config_t* config
)
{
    // =========================================================================
    // GUARD: Validate configuration
    // =========================================================================

    if (!config) {
        LOG_ERROR("NULL config in fault_working_memory_create_custom");
        return NULL;
    }

    // Validate capacity
    uint32_t capacity = config->max_capacity;
    if (capacity == 0) {
        LOG_WARNING("Zero capacity specified, using default: %u",
                   FAULT_WORKING_MEMORY_DEFAULT_CAPACITY);
        capacity = FAULT_WORKING_MEMORY_DEFAULT_CAPACITY;
    }

    if (capacity > FAULT_WORKING_MEMORY_MAX_CAPACITY) {
        LOG_WARNING("Capacity %u exceeds max %u, capping",
                   capacity, FAULT_WORKING_MEMORY_MAX_CAPACITY);
        capacity = FAULT_WORKING_MEMORY_MAX_CAPACITY;
    }

    // =========================================================================
    // ALLOCATION: Create working memory structure
    // =========================================================================

    fault_working_memory_t* wm = nimcp_malloc(sizeof(fault_working_memory_t));
    if (!wm) {
        LOG_ERROR("Failed to allocate fault_working_memory_t");
        return NULL;
    }

    // Allocate faults array
    wm->faults = nimcp_calloc(capacity, sizeof(active_fault_t));
    if (!wm->faults) {
        LOG_ERROR("Failed to allocate faults array");
        nimcp_free(wm);
        return NULL;
    }

    // =========================================================================
    // INITIALIZATION: Set initial state
    // =========================================================================

    wm->capacity = capacity;
    wm->count = 0;

    // Recovery state
    wm->active_strategy = RECOVERY_STRATEGY_NONE;
    wm->recovery_step = 0;
    wm->total_steps = 0;

    // Cascade detection
    wm->cascade_threshold = config->cascade_threshold;
    wm->cascade_window_us = config->cascade_window_us;
    wm->faults_in_window = 0;
    wm->window_start_us = fault_working_memory_get_timestamp_us();
    wm->cascade_detected = false;

    // Priority
    wm->priority_fault_idx = 0;

    LOG_INFO("Created fault working memory: capacity=%u, cascade_threshold=%u",
             capacity, wm->cascade_threshold);

    return wm;
}

void fault_working_memory_destroy(fault_working_memory_t* wm) {
    if (!wm) {
        return;  // Safe to call with NULL
    }

    if (wm->faults) {
        nimcp_free(wm->faults);
    }

    nimcp_free(wm);
}

//=============================================================================
// Fault Management Functions
//=============================================================================

bool fault_working_memory_add_fault(
    fault_working_memory_t* wm,
    const fault_t* fault
)
{
    // =========================================================================
    // GUARD: Validate parameters
    // =========================================================================

    if (!wm) {
        LOG_ERROR("NULL working memory in add_fault");
        return false;
    }

    if (!fault) {
        LOG_ERROR("NULL fault in add_fault");
        return false;
    }

    // =========================================================================
    // EVICTION: Make room if at capacity
    // =========================================================================

    if (wm->count >= wm->capacity) {
        // WHAT: Find and remove lowest-priority fault
        // WHY: Maintain capacity limit (Miller's Law)
        // HOW: Find eviction candidate, shift array
        uint32_t evict_idx = find_eviction_candidate(wm);

        LOG_DEBUG("Working memory full (%u/%u), evicting fault %u (severity=%d)",
                 wm->count, wm->capacity,
                 wm->faults[evict_idx].fault.fault_id,
                 wm->faults[evict_idx].fault.severity);

        // Shift array to remove evicted fault
        for (uint32_t i = evict_idx; i < wm->count - 1; i++) {
            wm->faults[i] = wm->faults[i + 1];
        }
        wm->count--;
    }

    // =========================================================================
    // INSERTION: Add new fault
    // =========================================================================

    uint32_t insert_idx = wm->count;

    // Copy fault data
    memcpy(&wm->faults[insert_idx].fault, fault, sizeof(fault_t));

    // Set metadata
    wm->faults[insert_idx].time_in_memory_us = fault_working_memory_get_timestamp_us();

    wm->count++;

    LOG_DEBUG("Added fault %u (severity=%d) to working memory: %u/%u",
             fault->fault_id, fault->severity, wm->count, wm->capacity);

    // =========================================================================
    // UPDATE: Cascade detection and priority
    // =========================================================================

    // Increment cascade counter
    wm->faults_in_window++;

    // Update priority fault
    update_priority_fault(wm);

    return true;
}

void fault_working_memory_remove_fault(
    fault_working_memory_t* wm,
    uint32_t fault_id
)
{
    if (!wm) {
        return;
    }

    // WHAT: Find fault by ID
    // WHY: Remove resolved fault
    // HOW: Linear search for ID
    uint32_t found_idx = wm->count;  // Invalid index
    for (uint32_t i = 0; i < wm->count; i++) {
        if (wm->faults[i].fault.fault_id == fault_id) {
            found_idx = i;
            break;
        }
    }

    if (found_idx >= wm->count) {
        LOG_DEBUG("Fault %u not found in working memory", fault_id);
        return;
    }

    // WHAT: Shift array to remove gap
    // WHY: Maintain contiguous array
    // HOW: Move elements left
    for (uint32_t i = found_idx; i < wm->count - 1; i++) {
        wm->faults[i] = wm->faults[i + 1];
    }

    wm->count--;

    LOG_DEBUG("Removed fault %u from working memory: %u/%u remaining",
             fault_id, wm->count, wm->capacity);

    // Update priority
    update_priority_fault(wm);
}

void fault_working_memory_clear(fault_working_memory_t* wm) {
    if (!wm) {
        return;
    }

    wm->count = 0;
    wm->faults_in_window = 0;
    wm->cascade_detected = false;
    wm->window_start_us = fault_working_memory_get_timestamp_us();

    // Reset recovery state
    wm->active_strategy = RECOVERY_STRATEGY_NONE;
    wm->recovery_step = 0;
    wm->total_steps = 0;

    LOG_INFO("Cleared fault working memory");
}

const active_fault_t* fault_working_memory_get_fault_at(
    const fault_working_memory_t* wm,
    uint32_t index
)
{
    if (!wm) {
        return NULL;
    }

    if (index >= wm->count) {
        return NULL;
    }

    return &wm->faults[index];
}

uint32_t fault_working_memory_get_count(const fault_working_memory_t* wm) {
    if (!wm) {
        return 0;
    }

    return wm->count;
}

//=============================================================================
// Priority and Attention Functions
//=============================================================================

active_fault_t* fault_working_memory_get_priority_fault(
    fault_working_memory_t* wm
)
{
    if (!wm) {
        return NULL;
    }

    if (wm->count == 0) {
        return NULL;
    }

    return &wm->faults[wm->priority_fault_idx];
}

//=============================================================================
// Recovery Progress Functions
//=============================================================================

bool fault_working_memory_set_recovery_strategy(
    fault_working_memory_t* wm,
    recovery_strategy_t strategy,
    uint32_t total_steps
)
{
    if (!wm) {
        LOG_ERROR("NULL working memory in set_recovery_strategy");
        return false;
    }

    wm->active_strategy = strategy;
    wm->total_steps = total_steps;
    wm->recovery_step = 0;

    LOG_INFO("Set recovery strategy: %s with %u steps",
             recovery_strategy_to_string(strategy), total_steps);

    return true;
}

void fault_working_memory_update_progress(
    fault_working_memory_t* wm,
    uint32_t step_completed
)
{
    if (!wm) {
        return;
    }

    wm->recovery_step = step_completed;

    LOG_DEBUG("Recovery progress: step %u/%u",
             wm->recovery_step, wm->total_steps);
}

uint32_t fault_working_memory_get_recovery_step(
    const fault_working_memory_t* wm
)
{
    if (!wm) {
        return 0;
    }

    return wm->recovery_step;
}

uint32_t fault_working_memory_get_total_steps(
    const fault_working_memory_t* wm
)
{
    if (!wm) {
        return 0;
    }

    return wm->total_steps;
}

//=============================================================================
// Cascade Detection Functions
//=============================================================================

bool fault_working_memory_is_cascading(const fault_working_memory_t* wm) {
    if (!wm) {
        return false;
    }

    return wm->cascade_detected;
}

void fault_working_memory_update_cascade_detection(fault_working_memory_t* wm) {
    if (!wm) {
        return;
    }

    uint64_t current_time = fault_working_memory_get_timestamp_us();
    uint64_t window_elapsed = current_time - wm->window_start_us;

    // WHAT: Check if time window has elapsed
    // WHY: Reset counter for new time window
    if (window_elapsed >= wm->cascade_window_us) {
        // New window
        wm->window_start_us = current_time;
        wm->faults_in_window = wm->count;  // Faults currently in memory
        wm->cascade_detected = false;
    }

    // WHAT: Check if cascade threshold exceeded
    // WHY: Detect cascading failure
    // HOW: Compare faults in window to threshold
    if (wm->faults_in_window >= wm->cascade_threshold) {
        if (!wm->cascade_detected) {
            LOG_WARNING("CASCADE DETECTED: %u faults in %llu us (threshold: %u)",
                       wm->faults_in_window,
                       (unsigned long long)window_elapsed,
                       wm->cascade_threshold);
        }
        wm->cascade_detected = true;
    } else {
        wm->cascade_detected = false;
    }
}

//=============================================================================
// Utility Functions
//=============================================================================

uint64_t fault_working_memory_get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

//=============================================================================
// String Conversion Functions
//=============================================================================

const char* fault_severity_to_string(fault_severity_t severity) {
    switch (severity) {
        case FAULT_SEVERITY_MINOR:    return "MINOR";
        case FAULT_SEVERITY_MAJOR:    return "MAJOR";
        case FAULT_SEVERITY_CRITICAL: return "CRITICAL";
        default:                      return "UNKNOWN";
    }
}

const char* recovery_strategy_to_string(recovery_strategy_t strategy) {
    switch (strategy) {
        case RECOVERY_STRATEGY_NONE:      return "NONE";
        case RECOVERY_STRATEGY_RETRY:     return "RETRY";
        case RECOVERY_STRATEGY_RESTART:   return "RESTART";
        case RECOVERY_STRATEGY_FAILOVER:  return "FAILOVER";
        case RECOVERY_STRATEGY_ROLLBACK:  return "ROLLBACK";
        case RECOVERY_STRATEGY_RESTORE:   return "RESTORE";
        case RECOVERY_STRATEGY_GRADUAL:   return "GRADUAL";
        case RECOVERY_STRATEGY_EMERGENCY: return "EMERGENCY";
        default:                          return "UNKNOWN";
    }
}
