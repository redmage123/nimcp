/**
 * @file nimcp_lgss_attention_guard.c
 * @brief LGSS Attention Safety Guard - Implementation
 * @version 1.0.0
 * @date 2026-01-16
 *
 * Implementation of attention monitoring and safety enforcement for
 * protecting against cognitive manipulation attacks targeting attention
 * allocation mechanisms.
 */

#include "security/lgss/cognitive/nimcp_lgss_attention_guard.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(lgss_attention_guard, MESH_ADAPTER_CATEGORY_SECURITY)


//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Safety target entry
 */
typedef struct {
    uint64_t target_id;
    uint32_t priority;
    bool is_registered;
} safety_target_entry_t;

/**
 * @brief Suspicious target entry
 */
typedef struct {
    uint64_t target_id;
    attention_target_class_t target_class;
    char reason[NIMCP_ERROR_BUFFER_MEDIUM];
    uint64_t marked_time;
} suspicious_target_entry_t;

/**
 * @brief Internal attention guard structure
 */
struct attention_guard {
    /* Configuration */
    attention_guard_config_t config;

    /* External system pointers */
    void* aix;
    void* attention_system;

    /* History tracking */
    attention_history_entry_t history[LGSS_ATTN_MAX_HISTORY];
    size_t history_count;
    size_t history_write_idx;

    /* Current state */
    uint64_t current_target_id;
    uint64_t current_focus_start_time;
    float current_safety_attention;

    /* Safety targets */
    safety_target_entry_t safety_targets[LGSS_ATTN_MAX_SAFETY_TARGETS];
    size_t safety_target_count;

    /* Suspicious targets */
    suspicious_target_entry_t suspicious_targets[LGSS_ATTN_MAX_SAFETY_TARGETS];
    size_t suspicious_count;

    /* Scatter detection */
    uint64_t recent_switches[32];
    size_t switch_count;
    uint64_t last_switch_time;

    /* Statistics */
    attention_guard_stats_t stats;

    /* Callback */
    attention_violation_callback_t callback;
    void* callback_user_data;

    /* Initialized flag */
    bool initialized;
};

//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Get current timestamp in milliseconds
 */
static uint64_t get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/**
 * @brief Add entry to attention history
 */
static void add_history_entry(attention_guard_t* guard,
                              uint64_t target_id,
                              float weight,
                              attention_target_class_t target_class) {
    attention_history_entry_t* entry = &guard->history[guard->history_write_idx];
    entry->target_id = target_id;
    entry->weight = weight;
    entry->start_time = get_current_time_ms();
    entry->end_time = 0;
    entry->target_class = target_class;

    guard->history_write_idx = (guard->history_write_idx + 1) % LGSS_ATTN_MAX_HISTORY;
    if (guard->history_count < LGSS_ATTN_MAX_HISTORY) {
        guard->history_count++;
    }
}

/**
 * @brief Check if target is registered as safety-relevant
 */
static bool is_safety_target(const attention_guard_t* guard, uint64_t target_id) {
    for (size_t i = 0; i < guard->safety_target_count; i++) {
        if (guard->safety_targets[i].is_registered &&
            guard->safety_targets[i].target_id == target_id) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Check if target is marked as suspicious
 */
static attention_target_class_t get_target_class(const attention_guard_t* guard,
                                                  uint64_t target_id) {
    /* Check safety targets first */
    for (size_t i = 0; i < guard->safety_target_count; i++) {
        if (guard->safety_targets[i].is_registered &&
            guard->safety_targets[i].target_id == target_id) {
            return ATTN_TARGET_SAFETY_CRITICAL;
        }
    }

    /* Check suspicious targets */
    for (size_t i = 0; i < guard->suspicious_count; i++) {
        if (guard->suspicious_targets[i].target_id == target_id) {
            return guard->suspicious_targets[i].target_class;
        }
    }

    return ATTN_TARGET_NORMAL;
}

/**
 * @brief Calculate total attention to safety-relevant items
 */
static float calculate_safety_attention(const attention_guard_t* guard,
                                         const attention_allocation_t* allocations,
                                         size_t num_allocations) {
    float total_safety = 0.0f;

    for (size_t i = 0; i < num_allocations; i++) {
        if (allocations[i].is_safety_relevant ||
            is_safety_target(guard, allocations[i].target_id)) {
            total_safety += allocations[i].attention_weight;
        }
    }

    return total_safety;
}

/**
 * @brief Invoke violation callback if registered
 */
static void invoke_callback(attention_guard_t* guard,
                            attention_safety_status_t status,
                            const attention_check_result_t* details) {
    if (guard->callback != NULL) {
        guard->callback(guard, status, details, guard->callback_user_data);
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

attention_guard_t* attention_guard_create(
    void* aix,
    void* attention_system,
    const attention_guard_config_t* config) {

    attention_guard_t* guard = (attention_guard_t*)nimcp_calloc(1, sizeof(attention_guard_t));
    if (guard == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "attention_guard_create: failed to allocate guard");

        return NULL;
    }

    /* Store external pointers */
    guard->aix = aix;
    guard->attention_system = attention_system;

    /* Initialize configuration */
    if (config != NULL) {
        guard->config = *config;
    } else {
        attention_guard_config_init_defaults(&guard->config);
    }

    /* Initialize state */
    guard->current_target_id = 0;
    guard->current_focus_start_time = 0;
    guard->current_safety_attention = 0.0f;

    guard->initialized = true;

    return guard;
}

void attention_guard_destroy(attention_guard_t* guard) {
    if (guard == NULL) {
        return;
    }

    memset(guard, 0, sizeof(attention_guard_t));
    nimcp_free(guard);
}

//=============================================================================
// Core Monitoring Functions
//=============================================================================

attention_guard_result_t attention_guard_check(
    attention_guard_t* guard,
    const attention_allocation_t* allocations,
    size_t num_allocations,
    attention_check_result_t* result) {

    if (guard == NULL || result == NULL) {
        return ATTN_GUARD_ERROR_NULL;
    }

    if (!guard->initialized) {
        return ATTN_GUARD_ERROR_NOT_INIT;
    }

    uint64_t current_time = get_current_time_ms();

    /* Initialize result */
    memset(result, 0, sizeof(attention_check_result_t));
    result->result = ATTN_GUARD_OK;
    result->status = ATTN_STATUS_NORMAL;

    guard->stats.total_checks++;
    guard->stats.last_check_time = current_time;

    if (num_allocations == 0 || allocations == NULL) {
        return ATTN_GUARD_OK;
    }

    /* Find primary attention target */
    uint64_t primary_target = 0;
    float max_weight = 0.0f;
    for (size_t i = 0; i < num_allocations; i++) {
        if (allocations[i].attention_weight > max_weight) {
            max_weight = allocations[i].attention_weight;
            primary_target = allocations[i].target_id;
        }
    }

    result->current_target_id = primary_target;

    /* Check for target switch */
    if (primary_target != guard->current_target_id) {
        /* Record switch for scatter detection */
        if (guard->switch_count < 32) {
            guard->recent_switches[guard->switch_count++] = current_time;
        } else {
            memmove(&guard->recent_switches[0], &guard->recent_switches[1],
                    31 * sizeof(uint64_t));
            guard->recent_switches[31] = current_time;
        }
        guard->last_switch_time = current_time;

        /* Update current focus */
        guard->current_target_id = primary_target;
        guard->current_focus_start_time = current_time;

        /* Add to history */
        add_history_entry(guard, primary_target, max_weight,
                          get_target_class(guard, primary_target));
    }

    result->current_focus_duration = current_time - guard->current_focus_start_time;

    /* Calculate safety attention */
    float safety_attention = calculate_safety_attention(guard, allocations, num_allocations);
    result->current_safety_attention = safety_attention;
    guard->current_safety_attention = safety_attention;

    /* Update average */
    guard->stats.avg_safety_attention =
        (guard->stats.avg_safety_attention * (guard->stats.total_checks - 1) + safety_attention)
        / guard->stats.total_checks;

    /* === HIJACKING DETECTION === */
    if (guard->config.monitor_hijacking) {
        bool is_hijacking = false;
        float confidence = 0.0f;

        /* Check for sudden large attention shift to suspicious target */
        attention_target_class_t target_class = get_target_class(guard, primary_target);
        if (target_class == ATTN_TARGET_SUSPICIOUS || target_class == ATTN_TARGET_DISTRACTOR) {
            if (max_weight > guard->config.hijack_detection_threshold) {
                is_hijacking = true;
                confidence = max_weight;
            }
        }

        /* Check history for sudden shift pattern */
        if (!is_hijacking && guard->history_count >= 2) {
            size_t prev_idx = (guard->history_write_idx + LGSS_ATTN_MAX_HISTORY - 1)
                              % LGSS_ATTN_MAX_HISTORY;
            float weight_delta = max_weight - guard->history[prev_idx].weight;
            if (weight_delta > guard->config.hijack_detection_threshold) {
                is_hijacking = true;
                confidence = weight_delta;
            }
        }

        if (is_hijacking) {
            result->status = ATTN_STATUS_HIJACKED;
            guard->stats.hijack_detections++;
            snprintf(result->violation_details, sizeof(result->violation_details),
                     "Hijacking detected: sudden attention shift (confidence=%.2f)", confidence);

            if (guard->config.log_violations) {
                invoke_callback(guard, ATTN_STATUS_HIJACKED, result);
            }
        }
    }

    /* === FIXATION DETECTION === */
    if (guard->config.monitor_fixation && result->status == ATTN_STATUS_NORMAL) {
        if (result->current_focus_duration > guard->config.max_attention_hold_ms) {
            result->status = ATTN_STATUS_FIXATED;
            guard->stats.fixation_detections++;
            snprintf(result->violation_details, sizeof(result->violation_details),
                     "Fixation detected: %llu ms on target %llu (max=%llu)",
                     (unsigned long long)result->current_focus_duration,
                     (unsigned long long)primary_target,
                     (unsigned long long)guard->config.max_attention_hold_ms);

            if (guard->config.log_violations) {
                invoke_callback(guard, ATTN_STATUS_FIXATED, result);
            }
        }
    }

    /* === SAFETY BLINDNESS DETECTION === */
    if (guard->config.monitor_safety_blindness && result->status == ATTN_STATUS_NORMAL) {
        if (safety_attention < guard->config.min_safety_attention) {
            result->status = ATTN_STATUS_SAFETY_BLIND;
            result->safety_enforcement_needed = true;
            guard->stats.safety_blind_detections++;
            snprintf(result->violation_details, sizeof(result->violation_details),
                     "Safety blindness: attention=%.2f (min=%.2f)",
                     safety_attention, guard->config.min_safety_attention);

            if (guard->config.log_violations) {
                invoke_callback(guard, ATTN_STATUS_SAFETY_BLIND, result);
            }
        }
    }

    /* === SCATTER DETECTION === */
    if (guard->config.monitor_scattering && result->status == ATTN_STATUS_NORMAL) {
        /* Count switches in detection window */
        uint32_t switches_in_window = 0;
        uint64_t window_start = current_time - guard->config.scatter_window_ms;

        for (size_t i = 0; i < guard->switch_count; i++) {
            if (guard->recent_switches[i] >= window_start) {
                switches_in_window++;
            }
        }

        if (switches_in_window >= guard->config.scatter_threshold_count) {
            result->status = ATTN_STATUS_SCATTERED;
            guard->stats.scatter_detections++;
            snprintf(result->violation_details, sizeof(result->violation_details),
                     "Attention scattering: %u switches in %u ms window",
                     switches_in_window, guard->config.scatter_window_ms);

            if (guard->config.log_violations) {
                invoke_callback(guard, ATTN_STATUS_SCATTERED, result);
            }
        }
    }

    /* === CHECK SINGLE TARGET ATTENTION LIMIT === */
    if (max_weight > guard->config.max_single_target_attention) {
        if (result->status == ATTN_STATUS_NORMAL) {
            result->status = ATTN_STATUS_HIJACKED;
        }
        guard->stats.blocked_allocations++;
        snprintf(result->violation_details, sizeof(result->violation_details),
                 "Excessive single target attention: %.2f (max=%.2f)",
                 max_weight, guard->config.max_single_target_attention);
    }

    return ATTN_GUARD_OK;
}

attention_guard_result_t attention_guard_ensure_safety_attention(
    attention_guard_t* guard,
    const attention_allocation_t* current_allocations,
    size_t num_allocations,
    attention_allocation_t* adjusted_allocations,
    size_t* adjustment_count) {

    if (guard == NULL || adjusted_allocations == NULL || adjustment_count == NULL) {
        return ATTN_GUARD_ERROR_NULL;
    }

    if (!guard->initialized) {
        return ATTN_GUARD_ERROR_NOT_INIT;
    }

    *adjustment_count = 0;

    if (num_allocations == 0 || current_allocations == NULL) {
        return ATTN_GUARD_OK;
    }

    /* Copy current allocations */
    memcpy(adjusted_allocations, current_allocations,
           num_allocations * sizeof(attention_allocation_t));

    /* Calculate current safety attention */
    float current_safety = calculate_safety_attention(guard, current_allocations, num_allocations);

    if (current_safety >= guard->config.min_safety_attention) {
        /* Safety attention is adequate */
        return ATTN_GUARD_OK;
    }

    /* Need to increase safety attention */
    float deficit = guard->config.min_safety_attention - current_safety;

    /* Find non-safety items to reduce */
    float total_non_safety = 0.0f;
    for (size_t i = 0; i < num_allocations; i++) {
        if (!current_allocations[i].is_safety_relevant &&
            !is_safety_target(guard, current_allocations[i].target_id)) {
            total_non_safety += current_allocations[i].attention_weight;
        }
    }

    if (total_non_safety < 0.001f) {
        /* Cannot reduce non-safety items further */
        return ATTN_GUARD_OK;
    }

    /* Calculate reduction ratio */
    float reduction_ratio = deficit / total_non_safety;
    if (reduction_ratio > 0.5f) {
        reduction_ratio = 0.5f;  /* Don't reduce by more than 50% */
    }

    /* Adjust allocations */
    size_t adj_count = 0;
    for (size_t i = 0; i < num_allocations; i++) {
        if (!current_allocations[i].is_safety_relevant &&
            !is_safety_target(guard, current_allocations[i].target_id)) {
            float reduction = adjusted_allocations[i].attention_weight * reduction_ratio;
            adjusted_allocations[i].attention_weight -= reduction;
            adj_count++;
        }
    }

    /* Increase safety item attention proportionally */
    float remaining_deficit = deficit;
    for (size_t i = 0; i < num_allocations && remaining_deficit > 0.0f; i++) {
        if (current_allocations[i].is_safety_relevant ||
            is_safety_target(guard, current_allocations[i].target_id)) {
            float increase = deficit / guard->safety_target_count;
            if (increase > remaining_deficit) {
                increase = remaining_deficit;
            }
            adjusted_allocations[i].attention_weight += increase;
            remaining_deficit -= increase;
            adj_count++;
        }
    }

    *adjustment_count = adj_count;
    guard->stats.safety_enforcements++;

    return ATTN_GUARD_OK;
}

attention_guard_result_t attention_guard_detect_hijacking(
    attention_guard_t* guard,
    const attention_allocation_t* new_allocation,
    bool* is_hijacking,
    float* confidence) {

    if (guard == NULL || new_allocation == NULL ||
        is_hijacking == NULL || confidence == NULL) {
        return ATTN_GUARD_ERROR_NULL;
    }

    if (!guard->initialized) {
        return ATTN_GUARD_ERROR_NOT_INIT;
    }

    *is_hijacking = false;
    *confidence = 0.0f;

    /* Check if target is already marked suspicious */
    attention_target_class_t target_class = get_target_class(guard, new_allocation->target_id);
    if (target_class == ATTN_TARGET_SUSPICIOUS || target_class == ATTN_TARGET_DISTRACTOR) {
        if (new_allocation->attention_weight > guard->config.hijack_detection_threshold) {
            *is_hijacking = true;
            *confidence = new_allocation->attention_weight;
            return ATTN_GUARD_OK;
        }
    }

    /* Check for sudden large shift from previous target */
    if (guard->history_count > 0) {
        size_t last_idx = (guard->history_write_idx + LGSS_ATTN_MAX_HISTORY - 1)
                          % LGSS_ATTN_MAX_HISTORY;
        attention_history_entry_t* last = &guard->history[last_idx];

        /* Different target with high weight shift */
        if (last->target_id != new_allocation->target_id) {
            float shift = new_allocation->attention_weight - last->weight;
            if (shift > guard->config.hijack_detection_threshold) {
                *is_hijacking = true;
                *confidence = shift;
            }
        }
    }

    return ATTN_GUARD_OK;
}

attention_guard_result_t attention_guard_detect_fixation(
    attention_guard_t* guard,
    uint64_t current_target,
    bool* is_fixated,
    uint64_t* fixation_duration) {

    if (guard == NULL || is_fixated == NULL || fixation_duration == NULL) {
        return ATTN_GUARD_ERROR_NULL;
    }

    if (!guard->initialized) {
        return ATTN_GUARD_ERROR_NOT_INIT;
    }

    *is_fixated = false;
    *fixation_duration = 0;

    if (current_target != guard->current_target_id) {
        /* Target has changed */
        return ATTN_GUARD_OK;
    }

    uint64_t duration = get_current_time_ms() - guard->current_focus_start_time;
    *fixation_duration = duration;

    if (duration > guard->config.max_attention_hold_ms) {
        *is_fixated = true;
    }

    return ATTN_GUARD_OK;
}

attention_guard_result_t attention_guard_detect_safety_blindness(
    attention_guard_t* guard,
    const uint64_t* safety_targets,
    size_t num_safety_targets,
    bool* is_blind,
    float* safety_attention) {

    if (guard == NULL || is_blind == NULL || safety_attention == NULL) {
        return ATTN_GUARD_ERROR_NULL;
    }

    if (!guard->initialized) {
        return ATTN_GUARD_ERROR_NOT_INIT;
    }

    *is_blind = false;
    *safety_attention = guard->current_safety_attention;

    if (*safety_attention < guard->config.min_safety_attention) {
        *is_blind = true;
    }

    /* Also check if specific safety targets are being ignored */
    if (safety_targets != NULL && num_safety_targets > 0) {
        /* Check recent history for safety target attention */
        size_t safety_attention_count = 0;
        size_t check_count = guard->history_count < 10 ? guard->history_count : 10;

        for (size_t i = 0; i < check_count; i++) {
            size_t idx = (guard->history_write_idx + LGSS_ATTN_MAX_HISTORY - 1 - i)
                         % LGSS_ATTN_MAX_HISTORY;

            for (size_t j = 0; j < num_safety_targets; j++) {
                if (guard->history[idx].target_id == safety_targets[j]) {
                    safety_attention_count++;
                    break;
                }
            }
        }

        /* If less than 20% of recent attention went to safety targets */
        if (check_count > 0 && (float)safety_attention_count / check_count < 0.2f) {
            *is_blind = true;
        }
    }

    return ATTN_GUARD_OK;
}

//=============================================================================
// Safety Target Management
//=============================================================================

attention_guard_result_t attention_guard_register_safety_target(
    attention_guard_t* guard,
    uint64_t target_id,
    uint32_t priority) {

    if (guard == NULL) {
        return ATTN_GUARD_ERROR_NULL;
    }

    if (!guard->initialized) {
        return ATTN_GUARD_ERROR_NOT_INIT;
    }

    /* Check if already registered */
    for (size_t i = 0; i < guard->safety_target_count; i++) {
        if (guard->safety_targets[i].is_registered &&
            guard->safety_targets[i].target_id == target_id) {
            /* Update priority */
            guard->safety_targets[i].priority = priority;
            return ATTN_GUARD_OK;
        }
    }

    /* Find empty slot */
    for (size_t i = 0; i < LGSS_ATTN_MAX_SAFETY_TARGETS; i++) {
        if (!guard->safety_targets[i].is_registered) {
            guard->safety_targets[i].target_id = target_id;
            guard->safety_targets[i].priority = priority;
            guard->safety_targets[i].is_registered = true;
            guard->safety_target_count++;
            return ATTN_GUARD_OK;
        }
    }

    return ATTN_GUARD_ERROR_CAPACITY;
}

attention_guard_result_t attention_guard_unregister_safety_target(
    attention_guard_t* guard,
    uint64_t target_id) {

    if (guard == NULL) {
        return ATTN_GUARD_ERROR_NULL;
    }

    if (!guard->initialized) {
        return ATTN_GUARD_ERROR_NOT_INIT;
    }

    for (size_t i = 0; i < LGSS_ATTN_MAX_SAFETY_TARGETS; i++) {
        if (guard->safety_targets[i].is_registered &&
            guard->safety_targets[i].target_id == target_id) {
            guard->safety_targets[i].is_registered = false;
            guard->safety_target_count--;
            return ATTN_GUARD_OK;
        }
    }

    return ATTN_GUARD_ERROR_INVALID;
}

attention_guard_result_t attention_guard_mark_suspicious(
    attention_guard_t* guard,
    uint64_t target_id,
    attention_target_class_t target_class,
    const char* reason) {

    if (guard == NULL) {
        return ATTN_GUARD_ERROR_NULL;
    }

    if (!guard->initialized) {
        return ATTN_GUARD_ERROR_NOT_INIT;
    }

    if (target_class != ATTN_TARGET_SUSPICIOUS && target_class != ATTN_TARGET_DISTRACTOR) {
        return ATTN_GUARD_ERROR_INVALID;
    }

    /* Check if already marked */
    for (size_t i = 0; i < guard->suspicious_count; i++) {
        if (guard->suspicious_targets[i].target_id == target_id) {
            /* Update */
            guard->suspicious_targets[i].target_class = target_class;
            if (reason != NULL) {
                strncpy(guard->suspicious_targets[i].reason, reason,
                        sizeof(guard->suspicious_targets[i].reason) - 1);
            }
            guard->suspicious_targets[i].marked_time = get_current_time_ms();
            return ATTN_GUARD_OK;
        }
    }

    if (guard->suspicious_count >= LGSS_ATTN_MAX_SAFETY_TARGETS) {
        return ATTN_GUARD_ERROR_CAPACITY;
    }

    /* Add new entry */
    suspicious_target_entry_t* entry = &guard->suspicious_targets[guard->suspicious_count];
    entry->target_id = target_id;
    entry->target_class = target_class;
    if (reason != NULL) {
        strncpy(entry->reason, reason, sizeof(entry->reason) - 1);
        entry->reason[sizeof(entry->reason) - 1] = '\0';
    } else {
        entry->reason[0] = '\0';
    }
    entry->marked_time = get_current_time_ms();
    guard->suspicious_count++;

    return ATTN_GUARD_OK;
}

//=============================================================================
// Configuration and Statistics
//=============================================================================

attention_guard_result_t attention_guard_set_config(
    attention_guard_t* guard,
    const attention_guard_config_t* config) {

    if (guard == NULL || config == NULL) {
        return ATTN_GUARD_ERROR_NULL;
    }

    if (!guard->initialized) {
        return ATTN_GUARD_ERROR_NOT_INIT;
    }

    guard->config = *config;
    return ATTN_GUARD_OK;
}

attention_guard_result_t attention_guard_get_config(
    const attention_guard_t* guard,
    attention_guard_config_t* config) {

    if (guard == NULL || config == NULL) {
        return ATTN_GUARD_ERROR_NULL;
    }

    if (!guard->initialized) {
        return ATTN_GUARD_ERROR_NOT_INIT;
    }

    *config = guard->config;
    return ATTN_GUARD_OK;
}

attention_guard_result_t attention_guard_get_stats(
    const attention_guard_t* guard,
    attention_guard_stats_t* stats) {

    if (guard == NULL || stats == NULL) {
        return ATTN_GUARD_ERROR_NULL;
    }

    if (!guard->initialized) {
        return ATTN_GUARD_ERROR_NOT_INIT;
    }

    *stats = guard->stats;
    return ATTN_GUARD_OK;
}

attention_guard_result_t attention_guard_reset_stats(attention_guard_t* guard) {
    if (guard == NULL) {
        return ATTN_GUARD_ERROR_NULL;
    }

    if (!guard->initialized) {
        return ATTN_GUARD_ERROR_NOT_INIT;
    }

    memset(&guard->stats, 0, sizeof(attention_guard_stats_t));
    return ATTN_GUARD_OK;
}

//=============================================================================
// Callback Registration
//=============================================================================

attention_guard_result_t attention_guard_register_callback(
    attention_guard_t* guard,
    attention_violation_callback_t callback,
    void* user_data) {

    if (guard == NULL) {
        return ATTN_GUARD_ERROR_NULL;
    }

    if (!guard->initialized) {
        return ATTN_GUARD_ERROR_NOT_INIT;
    }

    guard->callback = callback;
    guard->callback_user_data = user_data;
    return ATTN_GUARD_OK;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* attention_status_to_string(attention_safety_status_t status) {
    switch (status) {
        case ATTN_STATUS_NORMAL:       return "NORMAL";
        case ATTN_STATUS_HIJACKED:     return "HIJACKED";
        case ATTN_STATUS_FIXATED:      return "FIXATED";
        case ATTN_STATUS_SCATTERED:    return "SCATTERED";
        case ATTN_STATUS_SAFETY_BLIND: return "SAFETY_BLIND";
        default:                       return "UNKNOWN";
    }
}

const char* attention_guard_result_to_string(attention_guard_result_t result) {
    switch (result) {
        case ATTN_GUARD_OK:            return "OK";
        case ATTN_GUARD_ERROR_NULL:    return "ERROR_NULL";
        case ATTN_GUARD_ERROR_INVALID: return "ERROR_INVALID";
        case ATTN_GUARD_ERROR_MEMORY:  return "ERROR_MEMORY";
        case ATTN_GUARD_ERROR_CAPACITY: return "ERROR_CAPACITY";
        case ATTN_GUARD_ERROR_NOT_INIT: return "ERROR_NOT_INIT";
        case ATTN_GUARD_BLOCKED:       return "BLOCKED";
        default:                       return "UNKNOWN";
    }
}

void attention_guard_config_init_defaults(attention_guard_config_t* config) {
    if (config == NULL) {
        return;
    }

    config->monitor_hijacking = true;
    config->monitor_fixation = true;
    config->monitor_safety_blindness = true;
    config->monitor_scattering = true;

    config->max_single_target_attention = LGSS_ATTN_DEFAULT_MAX_SINGLE;
    config->min_safety_attention = LGSS_ATTN_DEFAULT_MIN_SAFETY;
    config->max_attention_hold_ms = LGSS_ATTN_DEFAULT_MAX_HOLD_MS;

    config->hijack_detection_threshold = LGSS_ATTN_HIJACK_THRESHOLD;
    config->scatter_window_ms = 5000;      /* 5 second window */
    config->scatter_threshold_count = 10;  /* 10 switches in window */

    config->auto_enforce_safety = true;
    config->log_violations = true;
}
