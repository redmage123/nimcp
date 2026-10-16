/**
 * @file nimcp_fault_attention.c
 * @brief Attention Mechanism for Error Prioritization Implementation
 * @version 1.0.0
 * @date 2025-11-20
 *
 * WHAT: Intelligent fault prioritization using attention-based weighting
 * WHY:  Limited resources require focusing on critical errors first
 * HOW:  Multi-factor scoring with adaptive weight learning
 */

#include "cognitive/fault_tolerance/nimcp_fault_attention.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"

#define LOG_MODULE "cognitive.fault.attention"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(fault_attention, MESH_ADAPTER_CATEGORY_COGNITIVE)

#define BIO_MODULE_COGNITIVE_FAULT_ATTENTION 0x0357


//=============================================================================
// Internal Structure Definition
//=============================================================================

/**
 * @brief Attention mechanism internal state
 */
struct fault_attention {
    // Configuration
    fault_attention_config_t config;

    // Computed attention weights
    float weights[FAULT_ATTENTION_MAX_FAULTS];
    uint32_t fault_count;

    // Focus tracking
    uint32_t focused_fault_idx;
    bool has_focus;

    // Last computation data (for adaptive learning)
    float last_severity_contribution[FAULT_ATTENTION_MAX_FAULTS];
    float last_recency_contribution[FAULT_ATTENTION_MAX_FAULTS];
    float last_frequency_contribution[FAULT_ATTENTION_MAX_FAULTS];
    float last_impact_contribution[FAULT_ATTENTION_MAX_FAULTS];

    // Statistics
    fault_attention_stats_t stats;
    struct timespec last_computation_time;

    // Bio-async integration
    bio_module_context_t bio_ctx;   /**< Bio-async module context */
    bool bio_async_enabled;         /**< Bio-async registration status */
};

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Validate configuration
 *
 * WHAT: Checks configuration validity
 * WHY:  Prevent invalid attention weights
 * HOW:  Verify weights sum to 1.0, all non-negative
 */
static bool validate_config_internal(const fault_attention_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "validate_config_internal: config is NULL");
        return false;
    }

    // Check all weights are non-negative
    if (config->severity_weight < 0.0F ||
        config->recency_weight < 0.0F ||
        config->frequency_weight < 0.0F ||
        config->impact_weight < 0.0F) {
        nimcp_log(LOG_LEVEL_ERROR, "Attention weights cannot be negative");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_config_internal: operation failed");
        return false;
    }

    // Check weights sum to approximately 1.0
    float sum = config->severity_weight + config->recency_weight +
                config->frequency_weight + config->impact_weight;

    if (fabsf(sum - 1.0F) > 0.01F) {
        nimcp_log(LOG_LEVEL_ERROR,
                  "Attention weights must sum to 1.0 (got %.3f)", sum);
        return false;
    }

    // Check learning rate in valid range
    if (config->learning_rate < 0.0F || config->learning_rate > 1.0F) {
        nimcp_log(LOG_LEVEL_ERROR,
                  "Learning rate must be in [0.0, 1.0] (got %.3f)",
                  config->learning_rate);
        return false;
    }

    // Check max tracked faults
    if (config->max_tracked_faults == 0 ||
        config->max_tracked_faults > FAULT_ATTENTION_MAX_FAULTS) {
        nimcp_log(LOG_LEVEL_ERROR,
                  "Max tracked faults must be in [1, %u] (got %u)",
                  FAULT_ATTENTION_MAX_FAULTS, config->max_tracked_faults);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_config_internal: operation failed");
        return false;
    }

    return true;
}

/**
 * @brief Normalize value to [0, 1] range
 *
 * WHAT: Maps value from [min, max] to [0, 1]
 * WHY:  Ensure all factors contribute on same scale
 * HOW:  Linear scaling with bounds checking
 */
static float normalize_value(float value, float min, float max) {
    if (max <= min) {
        return 0.0F;
    }

    float normalized = (value - min) / (max - min);

    // Clamp to [0, 1]
    if (normalized < 0.0F) normalized = 0.0F;
    if (normalized > 1.0F) normalized = 1.0F;

    return normalized;
}

/**
 * @brief Compute recency factor
 *
 * WHAT: Converts time delta to recency score
 * WHY:  Recent faults should have higher priority
 * HOW:  Inverse exponential decay: 1 / (1 + time_delta)
 */
static float compute_recency_factor(uint64_t time_delta_ms) {
    // Convert to seconds
    float time_delta_s = time_delta_ms / 1000.0F;

    // Use inverse decay: 1 / (1 + t)
    // This gives 1.0 for immediate (t=0), 0.5 for t=1s, etc.
    float recency = 1.0F / (1.0F + time_delta_s);

    return recency;
}

/**
 * @brief Find maximum value in array
 */
static uint32_t find_max_index(const float* values, uint32_t count) {
    if (count == 0) {
        return 0;
    }

    uint32_t max_idx = 0;
    float max_val = values[0];

    for (uint32_t i = 1; i < count; i++) {
        if (values[i] > max_val) {
            max_val = values[i];
            max_idx = i;
        }
    }

    return max_idx;
}

/**
 * @brief Identify dominant factor for a fault
 *
 * WHAT: Finds which factor contributed most to attention weight
 * WHY:  Needed for adaptive weight learning
 * HOW:  Compares contributions of severity, recency, frequency, impact
 */
static int find_dominant_factor(
    const fault_attention_t* attention,
    uint32_t fault_index
) {
    float contributions[4] = {
        attention->last_severity_contribution[fault_index],
        attention->last_recency_contribution[fault_index],
        attention->last_frequency_contribution[fault_index],
        attention->last_impact_contribution[fault_index]
    };

    return find_max_index(contributions, 4);
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

fault_attention_config_t fault_attention_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    fault_attention_heartbeat("fault_attent_default_config", 0.0f);


    fault_attention_config_t config = {};

    config.severity_weight = FAULT_ATTENTION_DEFAULT_SEVERITY_WEIGHT;
    config.recency_weight = FAULT_ATTENTION_DEFAULT_RECENCY_WEIGHT;
    config.frequency_weight = FAULT_ATTENTION_DEFAULT_FREQUENCY_WEIGHT;
    config.impact_weight = FAULT_ATTENTION_DEFAULT_IMPACT_WEIGHT;

    config.enable_adaptive_weights = false;
    config.learning_rate = FAULT_ATTENTION_DEFAULT_LEARNING_RATE;

    config.max_tracked_faults = FAULT_ATTENTION_MAX_FAULTS;
    config.min_attention_threshold = 0.0F;

    return config;
}

bool fault_attention_validate_config(const fault_attention_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    fault_attention_heartbeat("fault_attent_validate_config", 0.0f);


    return validate_config_internal(config);
}

fault_attention_t* fault_attention_create(void) {
    /* Phase 8: Heartbeat at operation start */
    fault_attention_heartbeat("fault_attent_create", 0.0f);


    LOG_DEBUG("Creating module");
    fault_attention_config_t config = fault_attention_default_config();
    return fault_attention_create_custom(&config);
}

fault_attention_t* fault_attention_create_custom(
    const fault_attention_config_t* config
) {
    // =========================================================================
    // GUARD: Validate parameters
    // =========================================================================

    /* Phase 8: Heartbeat at operation start */
    fault_attention_heartbeat("fault_attent_create_custom", 0.0f);


    fault_attention_config_t final_config;

    if (config == NULL) {
        // Use defaults if NULL
        final_config = fault_attention_default_config();
    } else {
        // Validate provided config
        if (!validate_config_internal(config)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "fault_attention_create_custom: invalid config");
            return NULL;
        }
        final_config = *config;
    }

    // =========================================================================
    // ALLOCATION: Create attention structure
    // =========================================================================

    fault_attention_t* attention =
        (fault_attention_t*)nimcp_calloc(1, sizeof(fault_attention_t));

    if (!attention) {
        nimcp_log(LOG_LEVEL_ERROR,
                  "Failed to allocate fault_attention_t (%zu bytes)",
                  sizeof(fault_attention_t));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fault_attention_create_custom: failed to allocate attention");


        return NULL;
    }

    // =========================================================================
    // INITIALIZATION: Set up state
    // =========================================================================

    attention->config = final_config;
    attention->fault_count = 0;
    attention->focused_fault_idx = 0;
    attention->has_focus = false;

    // Initialize weights to zero
    memset(attention->weights, 0, sizeof(attention->weights));

    // Initialize contribution tracking
    memset(attention->last_severity_contribution, 0,
           sizeof(attention->last_severity_contribution));
    memset(attention->last_recency_contribution, 0,
           sizeof(attention->last_recency_contribution));
    memset(attention->last_frequency_contribution, 0,
           sizeof(attention->last_frequency_contribution));
    memset(attention->last_impact_contribution, 0,
           sizeof(attention->last_impact_contribution));

    // Initialize statistics
    memset(&attention->stats, 0, sizeof(attention->stats));

    nimcp_log(LOG_LEVEL_INFO,
              "Fault attention mechanism created (severity=%.2f, recency=%.2f, "
              "frequency=%.2f, impact=%.2f, adaptive=%d)",
              final_config.severity_weight,
              final_config.recency_weight,
              final_config.frequency_weight,
              final_config.impact_weight,
              final_config.enable_adaptive_weights);

    
    // Bio-async registration
    attention->bio_ctx = NULL;
    attention->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_ATTENTION_FAULT,
            .module_name = "fault_attention",
            .inbox_capacity = 32,
            .user_data = attention
        };
        attention->bio_ctx = bio_router_register_module(&bio_info);
        if (attention->bio_ctx) {
            attention->bio_async_enabled = true;
        }
    }

return attention;
}

void fault_attention_destroy(fault_attention_t* attention) {
    /* Phase 8: Heartbeat at operation start */
    fault_attention_heartbeat("fault_attent_destroy", 0.0f);


    LOG_DEBUG("Destroying module");
    if (!attention) {
        return;
    }

    nimcp_log(LOG_LEVEL_DEBUG,
              "Destroying fault attention (computations=%lu, updates=%lu)",
              attention->stats.total_computations,
              attention->stats.total_updates);

    // Unregister from bio-router
    if (attention->bio_async_enabled && attention->bio_ctx) {
        bio_router_unregister_module(attention->bio_ctx);
        attention->bio_ctx = NULL;
        attention->bio_async_enabled = false;
    }

    nimcp_free(attention);
    attention = NULL;
}

//=============================================================================
// Attention Computation Functions
//=============================================================================

bool fault_attention_compute_weights(
    fault_attention_t* attention,
    const active_fault_t* faults,
    uint32_t fault_count,
    uint64_t current_time_ms
) {
    // =========================================================================
    // GUARD: Validate parameters
    // =========================================================================

    if (!attention) {
        nimcp_log(LOG_LEVEL_ERROR, "NULL attention in compute_weights");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fault_attention_compute_weights: attention is NULL");
        return false;
    }

    // Process pending bio-async messages
    /* Phase 8: Heartbeat at operation start */
    fault_attention_heartbeat("fault_attent_compute_weights", 0.0f);


    if (attention->bio_async_enabled && attention->bio_ctx) {
        bio_router_process_inbox(attention->bio_ctx, 5);
    }

    if (fault_count > 0 && !faults) {
        nimcp_log(LOG_LEVEL_ERROR, "NULL faults with non-zero count");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fault_attention_compute_weights: faults is NULL");
        return false;
    }

    if (fault_count > attention->config.max_tracked_faults) {
        nimcp_log(LOG_LEVEL_ERROR,
                  "Fault count %u exceeds maximum %u",
                  fault_count, attention->config.max_tracked_faults);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "fault_attention_compute_weights: validation failed");
        return false;
    }

    // =========================================================================
    // SPECIAL CASE: No faults
    // =========================================================================

    if (fault_count == 0) {
        attention->fault_count = 0;
        attention->has_focus = false;
        return true;
    }

    // =========================================================================
    // TIMING: Start performance measurement
    // =========================================================================

    struct timespec start_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    // =========================================================================
    // NORMALIZATION: Find min/max for each factor
    // =========================================================================

    // Track min/max for normalization
    float max_severity = 0.0F;
    uint32_t max_occurrence = 0;
    uint32_t max_users = 0;
    uint64_t min_time_delta = UINT64_MAX;
    uint64_t max_time_delta = 0;

    for (uint32_t i = 0; i < fault_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && fault_count > 256) {
            fault_attention_heartbeat("fault_attent_loop",
                             (float)(i + 1) / (float)fault_count);
        }

        const active_fault_t* fault = &faults[i];

        // Severity is already normalized [0, 1]
        if (fault->severity > max_severity) {
            max_severity = fault->severity;
        }

        // Occurrence count
        if (fault->occurrence_count > max_occurrence) {
            max_occurrence = fault->occurrence_count;
        }

        // Users affected
        if (fault->users_affected > max_users) {
            max_users = fault->users_affected;
        }

        // Time delta for recency
        if (current_time_ms >= fault->last_occurrence_ms) {
            uint64_t delta = current_time_ms - fault->last_occurrence_ms;
            if (delta < min_time_delta) min_time_delta = delta;
            if (delta > max_time_delta) max_time_delta = delta;
        }
    }

    // =========================================================================
    // COMPUTATION: Calculate attention weights
    // =========================================================================

    for (uint32_t i = 0; i < fault_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && fault_count > 256) {
            fault_attention_heartbeat("fault_attent_loop",
                             (float)(i + 1) / (float)fault_count);
        }

        const active_fault_t* fault = &faults[i];

        // Severity factor (already [0, 1])
        float severity_factor = fault->severity;

        // Recency factor (inverse of time since)
        uint64_t time_delta = (current_time_ms >= fault->last_occurrence_ms) ?
                             (current_time_ms - fault->last_occurrence_ms) : 0;
        float recency_factor = compute_recency_factor(time_delta);

        // Frequency factor (normalized occurrence count)
        float frequency_factor = (max_occurrence > 0) ?
            normalize_value(fault->occurrence_count, 0, max_occurrence) : 0.0F;

        // Impact factor (normalized users affected)
        float impact_factor = (max_users > 0) ?
            normalize_value(fault->users_affected, 0, max_users) : 0.0F;

        // Store contributions for adaptive learning
        attention->last_severity_contribution[i] =
            attention->config.severity_weight * severity_factor;
        attention->last_recency_contribution[i] =
            attention->config.recency_weight * recency_factor;
        attention->last_frequency_contribution[i] =
            attention->config.frequency_weight * frequency_factor;
        attention->last_impact_contribution[i] =
            attention->config.impact_weight * impact_factor;

        // Compute weighted sum
        attention->weights[i] =
            attention->last_severity_contribution[i] +
            attention->last_recency_contribution[i] +
            attention->last_frequency_contribution[i] +
            attention->last_impact_contribution[i];
    }

    // =========================================================================
    // NORMALIZATION: Normalize weights to [0, 1]
    // =========================================================================

    float max_weight = 0.0F;
    for (uint32_t i = 0; i < fault_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && fault_count > 256) {
            fault_attention_heartbeat("fault_attent_loop",
                             (float)(i + 1) / (float)fault_count);
        }

        if (attention->weights[i] > max_weight) {
            max_weight = attention->weights[i];
        }
    }

    if (max_weight > 0.0F) {
        for (uint32_t i = 0; i < fault_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && fault_count > 256) {
                fault_attention_heartbeat("fault_attent_loop",
                                 (float)(i + 1) / (float)fault_count);
            }

            attention->weights[i] /= max_weight;
        }
    }

    // =========================================================================
    // FOCUS: Update focused fault index
    // =========================================================================

    attention->fault_count = fault_count;
    attention->focused_fault_idx = find_max_index(attention->weights, fault_count);
    attention->has_focus = true;

    // =========================================================================
    // STATISTICS: Update stats
    // =========================================================================

    attention->stats.total_computations++;
    attention->stats.current_fault_count = fault_count;
    attention->stats.max_attention_weight = max_weight;

    if (fault_count > 0) {
        attention->stats.focused_fault_id = faults[attention->focused_fault_idx].fault_id;
    }

    // Compute elapsed time
    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    uint64_t elapsed_ns = (end_time.tv_sec - start_time.tv_sec) * 1000000000ULL +
                         (end_time.tv_nsec - start_time.tv_nsec);
    float elapsed_us = elapsed_ns / 1000.0F;

    // Update average computation time (exponential moving average)
    if (attention->stats.total_computations == 1) {
        attention->stats.avg_computation_time_us = elapsed_us;
    } else {
        attention->stats.avg_computation_time_us =
            0.9F * attention->stats.avg_computation_time_us + 0.1F * elapsed_us;
    }

    attention->last_computation_time = end_time;

    return true;
}

bool fault_attention_get_weight(
    const fault_attention_t* attention,
    uint32_t fault_index,
    float* weight
) {
    // =========================================================================
    // GUARD: Validate parameters
    // =========================================================================

    if (!attention) {
        nimcp_log(LOG_LEVEL_ERROR, "NULL attention in get_weight");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fault_attention_get_weight: attention is NULL");
        return false;
    }

    if (!weight) {
        nimcp_log(LOG_LEVEL_ERROR, "NULL weight output in get_weight");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fault_attention_get_weight: weight is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    fault_attention_heartbeat("fault_attent_get_weight", 0.0f);


    if (fault_index >= attention->fault_count) {
        nimcp_log(LOG_LEVEL_ERROR,
                  "Fault index %u out of bounds (count=%u)",
                  fault_index, attention->fault_count);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "fault_attention_get_weight: capacity exceeded");
        return false;
    }

    // =========================================================================
    // RETRIEVAL: Return weight
    // =========================================================================

    *weight = attention->weights[fault_index];
    return true;
}

uint32_t fault_attention_get_all_weights(
    const fault_attention_t* attention,
    float* weights,
    uint32_t max_count
) {
    // =========================================================================
    // GUARD: Validate parameters
    // =========================================================================

    if (!attention || !weights || max_count == 0) {
        return 0;
    }

    // =========================================================================
    // COPY: Copy weights to output
    // =========================================================================

    /* Phase 8: Heartbeat at operation start */
    fault_attention_heartbeat("fault_attent_get_all_weights", 0.0f);


    uint32_t count = (attention->fault_count < max_count) ?
                     attention->fault_count : max_count;

    memcpy(weights, attention->weights, count * sizeof(float));

    return count;
}

//=============================================================================
// Focus Management Functions
//=============================================================================

bool fault_attention_get_focused_index(
    const fault_attention_t* attention,
    uint32_t* focused_index
) {
    // =========================================================================
    // GUARD: Validate parameters
    // =========================================================================

    if (!attention) {
        nimcp_log(LOG_LEVEL_ERROR, "NULL attention in get_focused_index");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fault_attention_get_focused_index: attention is NULL");
        return false;
    }

    if (!focused_index) {
        nimcp_log(LOG_LEVEL_ERROR, "NULL focused_index output");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fault_attention_get_focused_index: focused_index is NULL");
        return false;
    }

    if (!attention->has_focus || attention->fault_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "fault_attention_get_focused_index: no focus set");
        return false;
    }

    // =========================================================================
    // RETRIEVAL: Return focused index
    // =========================================================================

    *focused_index = attention->focused_fault_idx;
    /* Phase 8: Heartbeat at operation start */
    fault_attention_heartbeat("fault_attent_get_focused_index", 0.0f);


    return true;
}

bool fault_attention_get_focused_fault(
    const fault_attention_t* attention,
    const active_fault_t* faults,
    uint32_t fault_count,
    active_fault_t* focused_fault
) {
    // =========================================================================
    // GUARD: Validate parameters
    // =========================================================================

    if (!attention || !faults || !focused_fault) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fault_attention_get_focused_fault: required parameter is NULL (attention, faults, focused_fault)");
        return false;
    }

    if (!attention->has_focus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "fault_attention_get_focused_fault: no focus set");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    fault_attention_heartbeat("fault_attent_get_focused_fault", 0.0f);


    if (attention->focused_fault_idx >= fault_count) {
        nimcp_log(LOG_LEVEL_ERROR,
                  "Focused index %u out of bounds (count=%u)",
                  attention->focused_fault_idx, fault_count);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "fault_attention_get_focused_fault: capacity exceeded");
        return false;
    }

    // =========================================================================
    // COPY: Copy focused fault
    // =========================================================================

    *focused_fault = faults[attention->focused_fault_idx];
    return true;
}

//=============================================================================
// Adaptive Learning Functions
//=============================================================================

bool fault_attention_update_weights(
    fault_attention_t* attention,
    uint32_t fault_index,
    bool recovery_success
) {
    // =========================================================================
    // GUARD: Validate parameters
    // =========================================================================

    if (!attention) {
        nimcp_log(LOG_LEVEL_ERROR, "NULL attention in update_weights");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fault_attention_update_weights: attention is NULL");
        return false;
    }

    if (!attention->config.enable_adaptive_weights) {
        // Adaptive learning disabled, silently succeed
        return true;
    }

    /* Phase 8: Heartbeat at operation start */
    fault_attention_heartbeat("fault_attent_update_weights", 0.0f);


    if (fault_index >= attention->fault_count) {
        nimcp_log(LOG_LEVEL_ERROR,
                  "Fault index %u out of bounds in update_weights",
                  fault_index);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "fault_attention_update_weights: capacity exceeded");
        return false;
    }

    // =========================================================================
    // LEARNING: Identify dominant factor
    // =========================================================================

    int dominant = find_dominant_factor(attention, fault_index);

    float learning_rate = attention->config.learning_rate;
    float* weights[4] = {
        &attention->config.severity_weight,
        &attention->config.recency_weight,
        &attention->config.frequency_weight,
        &attention->config.impact_weight
    };

    // =========================================================================
    // UPDATE: Adjust weights based on outcome
    // =========================================================================

    if (recovery_success) {
        // WHAT: Success reinforces dominant factor
        // WHY:  Learn which factors predict successful recovery
        // HOW:  Increase dominant weight, decrease others proportionally

        *weights[dominant] += learning_rate;

        // Decrease other weights proportionally
        for (int i = 0; i < 4; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && 4 > 256) {
                fault_attention_heartbeat("fault_attent_loop",
                                 (float)(i + 1) / (float)4);
            }

            if (i != dominant) {
                *weights[i] -= learning_rate / 3.0F;
            }
        }
    } else {
        // WHAT: Failure reduces dominant factor importance
        // WHY:  Dominant factor didn't lead to success
        // HOW:  Decrease dominant weight, increase others

        *weights[dominant] -= learning_rate;

        // Increase other weights proportionally
        for (int i = 0; i < 4; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && 4 > 256) {
                fault_attention_heartbeat("fault_attent_loop",
                                 (float)(i + 1) / (float)4);
            }

            if (i != dominant) {
                *weights[i] += learning_rate / 3.0F;
            }
        }
    }

    // =========================================================================
    // NORMALIZATION: Ensure weights sum to 1.0 and are non-negative
    // =========================================================================

    // Clamp to non-negative
    for (int i = 0; i < 4; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 4 > 256) {
            fault_attention_heartbeat("fault_attent_loop",
                             (float)(i + 1) / (float)4);
        }

        if (*weights[i] < 0.01F) {
            *weights[i] = 0.01F; // Minimum weight
        }
    }

    // Normalize to sum to 1.0
    float sum = attention->config.severity_weight +
                attention->config.recency_weight +
                attention->config.frequency_weight +
                attention->config.impact_weight;

    if (sum > 0.0F) {
        attention->config.severity_weight /= sum;
        attention->config.recency_weight /= sum;
        attention->config.frequency_weight /= sum;
        attention->config.impact_weight /= sum;
    }

    // =========================================================================
    // STATISTICS: Update stats
    // =========================================================================

    attention->stats.total_updates++;

    nimcp_log(LOG_LEVEL_DEBUG,
              "Adaptive weight update (success=%d, dominant=%d): "
              "severity=%.3f, recency=%.3f, frequency=%.3f, impact=%.3f",
              recovery_success, dominant,
              attention->config.severity_weight,
              attention->config.recency_weight,
              attention->config.frequency_weight,
              attention->config.impact_weight);

    return true;
}

bool fault_attention_reset_weights(fault_attention_t* attention) {
    if (!attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fault_attention_reset_weights: attention is NULL");
        return false;
    }

    // Reset to defaults
    /* Phase 8: Heartbeat at operation start */
    fault_attention_heartbeat("fault_attent_reset_weights", 0.0f);


    fault_attention_config_t defaults = fault_attention_default_config();
    attention->config.severity_weight = defaults.severity_weight;
    attention->config.recency_weight = defaults.recency_weight;
    attention->config.frequency_weight = defaults.frequency_weight;
    attention->config.impact_weight = defaults.impact_weight;

    nimcp_log(LOG_LEVEL_INFO, "Attention weights reset to defaults");

    return true;
}

//=============================================================================
// Configuration Functions
//=============================================================================

bool fault_attention_get_config(
    const fault_attention_t* attention,
    fault_attention_config_t* config
) {
    if (!attention || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fault_attention_get_config: required parameter is NULL (attention, config)");
        return false;
    }

    *config = attention->config;
    /* Phase 8: Heartbeat at operation start */
    fault_attention_heartbeat("fault_attent_get_config", 0.0f);


    return true;
}

bool fault_attention_set_config(
    fault_attention_t* attention,
    const fault_attention_config_t* config
) {
    if (!attention || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fault_attention_set_config: required parameter is NULL (attention, config)");
        return false;
    }

    if (!validate_config_internal(config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "fault_attention_set_config: invalid config");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    fault_attention_heartbeat("fault_attent_set_config", 0.0f);


    attention->config = *config;

    nimcp_log(LOG_LEVEL_INFO, "Attention config updated");

    return true;
}

//=============================================================================
// Statistics Functions
//=============================================================================

bool fault_attention_get_stats(
    const fault_attention_t* attention,
    fault_attention_stats_t* stats
) {
    if (!attention || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fault_attention_get_stats: required parameter is NULL (attention, stats)");
        return false;
    }

    *stats = attention->stats;
    /* Phase 8: Heartbeat at operation start */
    fault_attention_heartbeat("fault_attent_get_stats", 0.0f);


    return true;
}

bool fault_attention_reset_stats(fault_attention_t* attention) {
    if (!attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fault_attention_reset_stats: attention is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    fault_attention_heartbeat("fault_attent_reset_stats", 0.0f);


    memset(&attention->stats, 0, sizeof(attention->stats));

    nimcp_log(LOG_LEVEL_DEBUG, "Attention statistics reset");

    return true;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int fault_attention_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    fault_attention_heartbeat("fault_attent_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Fault_Attention");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                fault_attention_heartbeat("fault_attent_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            nimcp_log(LOG_LEVEL_DEBUG, "[KG-Self] %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Fault_Attention");
    if (connections) {
        for (uint32_t i = 0; i < connections->count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && connections->count > 256) {
                fault_attention_heartbeat("fault_attent_loop",
                                 (float)(i + 1) / (float)connections->count);
            }

            nimcp_log(LOG_LEVEL_DEBUG, "[KG-Rel] -> %s (%s)",
                      connections->relations[i]->to,
                      connections->relations[i]->relation_type);
        }
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Fault_Attention");
    if (incoming) {
        for (uint32_t i = 0; i < incoming->count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && incoming->count > 256) {
                fault_attention_heartbeat("fault_attent_loop",
                                 (float)(i + 1) / (float)incoming->count);
            }

            nimcp_log(LOG_LEVEL_DEBUG, "[KG-Rel] <- %s (%s)",
                      incoming->relations[i]->from,
                      incoming->relations[i]->relation_type);
        }
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void fault_attention_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_fault_attention_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Functions (FULL implementation)
 * ============================================================================ */
int fault_attention_training_begin(fault_attention_t* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fault_attention_training_begin: NULL argument");
        return -1;
    }
    struct fault_attention* ctx = (struct fault_attention*)instance;
    fault_attention_heartbeat_instance(NULL, "fault_attent_training_begin", 0.0f);
    memset(&ctx->stats, 0, sizeof(ctx->stats));
    ctx->fault_count = 0;
    ctx->has_focus = false;
    ctx->focused_fault_idx = 0;
    NIMCP_LOGGING_INFO("%s training begin: counters reset", "fault_attention");
    return 0;
}

int fault_attention_training_step(fault_attention_t* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fault_attention_training_step: NULL argument");
        return -1;
    }
    struct fault_attention* ctx = (struct fault_attention*)instance;
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    fault_attention_heartbeat_instance(NULL, "fault_attent_training_step", progress);
    ctx->stats.total_computations++;
    /* Adaptive attention decay during training */
    float decay = 1.0f - 0.1f * progress;
    if (decay < 0.5f) decay = 0.5f;
    for (uint32_t i = 0; i < ctx->fault_count; i++) {
        ctx->weights[i] *= decay;
    }
    return 0;
}

int fault_attention_training_end(fault_attention_t* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fault_attention_training_end: NULL argument");
        return -1;
    }
    struct fault_attention* ctx = (struct fault_attention*)instance;
    fault_attention_heartbeat_instance(NULL, "fault_attent_training_end", 1.0f);
    NIMCP_LOGGING_INFO("%s training end: %u faults, %lu computations",
                       "fault_attention", ctx->fault_count,
                       (unsigned long)ctx->stats.total_computations);
    return 0;
}
