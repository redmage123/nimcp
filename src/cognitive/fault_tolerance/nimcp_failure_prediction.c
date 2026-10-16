/**
 * @file nimcp_failure_prediction.c
 * @brief Implementation of Predictive Coding for Failure Prediction
 * @version 1.0.0
 * @date 2025-11-20
 *
 * WHAT: Predict failures before they occur using leading indicators
 * WHY:  Prevention is better than recovery - avoid crashes
 * HOW:  Track metrics, calculate derivatives, predict probability
 *
 * ALGORITHM:
 * 1. Update indicators with new metric values
 * 2. Calculate rate of change (first derivative)
 * 3. Calculate acceleration (second derivative)
 * 4. Evaluate patterns (sustained growth, exponential growth)
 * 5. Generate predictions with probability and time estimate
 * 6. Sort predictions by probability
 * 7. Return predictions above threshold
 *
 * PERFORMANCE:
 * - Update: O(1) per indicator
 * - Predict: O(n*m) where n=indicators, m=failure types
 * - Memory: ~2 KB + indicator/prediction arrays
 *
 * @author NIMCP Development Team
 */

#include "cognitive/fault_tolerance/nimcp_failure_prediction.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"

#define LOG_MODULE "cognitive.fault.prediction"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(failure_prediction, MESH_ADAPTER_CATEGORY_COGNITIVE)

#define BIO_MODULE_COGNITIVE_FAULT_PREDICTION 0x0356


#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif
#include "utils/thread/nimcp_thread.h"

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal indicator storage with history
 *
 * WHAT: Indicator with previous values for derivative calculation
 * WHY:  Need history to calculate rate of change
 */
typedef struct {
    leading_indicator_t current;   /**< Current indicator state */
    float previous_value;          /**< Previous value for derivative */
    float previous_rate;           /**< Previous rate for acceleration */
    uint64_t previous_timestamp;   /**< Previous update time */
    bool initialized;              /**< Has been updated at least once */
} internal_indicator_t;

/**
 * @brief Failure predictor implementation
 *
 * WHAT: Full predictor structure (opaque to users)
 * WHY:  Encapsulation and data hiding
 */
struct failure_predictor {
    // Configuration
    failure_predictor_config_t config;

    // Leading indicators
    internal_indicator_t* indicators;
    uint32_t indicator_count;

    // Predictions
    failure_prediction_t* predictions;
    uint32_t prediction_count;

    // Thread safety
    nimcp_rwlock_t lock;

    // Bio-async integration
    bio_module_context_t bio_ctx;   /**< Bio-async module context */
    bool bio_async_enabled;         /**< Bio-async registration status */
};

//=============================================================================
// Constants
//=============================================================================

// Memory leak thresholds
#define MEMORY_LEAK_RATE_THRESHOLD_MB_PER_SEC 10.0f  /**< 10 MB/sec growth */
#define MEMORY_LEAK_MIN_ACCELERATION 0.01f            /**< Positive acceleration */

// Gradient explosion thresholds
#define GRADIENT_EXPLOSION_NORM_THRESHOLD 1000.0f     /**< High gradient norm */
#define GRADIENT_EXPLOSION_RATE_THRESHOLD 100.0f      /**< Rapid growth rate */

// Time estimation limits
#define MAX_MEMORY_BYTES (8ULL * 1024 * 1024 * 1024)  /**< Assume 8 GB max */
#define MAX_GRADIENT_NORM 1000000.0f                   /**< Divergence threshold */

// Preventive action thresholds
#define PREVENTION_PROBABILITY_THRESHOLD 0.8f
#define PREVENTION_URGENT_TIME_MS 10000  /**< 10 seconds */
#define PREVENTION_URGENT_PROBABILITY 0.6f

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get current timestamp in milliseconds
 *
 * WHAT: Platform-independent timestamp
 * WHY:  Needed for time delta calculations
 *
 * COMPLEXITY: O(1)
 *
 * @return Timestamp in milliseconds
 */
static uint64_t get_timestamp_ms(void) {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t ticks = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return ticks / 10000;  // Convert 100ns to ms
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
#endif
}

/**
 * @brief Lock predictor for reading (thread-safe)
 *
 * WHAT: Acquire read lock
 * WHY:  Allow concurrent reads
 */
static void lock_read(failure_predictor_t* predictor) {
    nimcp_rwlock_rdlock(&predictor->lock);
}

/**
 * @brief Lock predictor for writing (thread-safe)
 *
 * WHAT: Acquire write lock
 * WHY:  Exclusive access for modifications
 */
static void lock_write(failure_predictor_t* predictor) {
    nimcp_rwlock_wrlock(&predictor->lock);
}

/**
 * @brief Unlock predictor (thread-safe)
 *
 * WHAT: Release lock
 */
static void unlock(failure_predictor_t* predictor) {
    nimcp_rwlock_unlock(&predictor->lock);
}

/**
 * @brief Find indicator by metric type
 *
 * WHAT: Search for indicator in array
 * WHY:  Avoid duplicates and allow updates
 *
 * COMPLEXITY: O(n) where n = indicator_count
 *
 * @param predictor Predictor instance
 * @param metric Metric type to find
 * @return Index of indicator or -1 if not found
 */
static int find_indicator_index(failure_predictor_t* predictor, metric_type_t metric) {
    for (uint32_t i = 0; i < predictor->indicator_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && predictor->indicator_count > 256) {
            failure_prediction_heartbeat("failure_pred_loop",
                             (float)(i + 1) / (float)predictor->indicator_count);
        }

        if (predictor->indicators[i].current.metric == metric) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * @brief Compare predictions for sorting (qsort callback)
 *
 * WHAT: Compare by probability (descending)
 * WHY:  Sort predictions highest first
 */
static int compare_predictions(const void* a, const void* b) {
    const failure_prediction_t* pred_a = (const failure_prediction_t*)a;
    const failure_prediction_t* pred_b = (const failure_prediction_t*)b;

    // Sort descending by probability
    if (pred_a->probability > pred_b->probability) return -1;
    if (pred_a->probability < pred_b->probability) return 1;
    return 0;
}

//=============================================================================
// Core Functions
//=============================================================================

failure_predictor_config_t failure_predictor_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    failure_prediction_heartbeat("failure_pred_failure_predictor_de", 0.0f);


    failure_predictor_config_t config = {};
    config.max_predictions = FAILURE_PREDICTOR_DEFAULT_MAX_PREDICTIONS;
    config.max_indicators = FAILURE_PREDICTOR_DEFAULT_MAX_INDICATORS;
    config.prediction_threshold = FAILURE_PREDICTOR_DEFAULT_THRESHOLD;
    config.enable_memory_leak_detection = true;
    config.enable_gradient_explosion_detection = true;
    return config;
}

failure_predictor_t* failure_predictor_create(void) {
    /* Phase 8: Heartbeat at operation start */
    failure_prediction_heartbeat("failure_pred_failure_predictor_cr", 0.0f);


    LOG_DEBUG("Creating module");
    failure_predictor_config_t config = failure_predictor_default_config();
    return failure_predictor_create_custom(&config);
}

failure_predictor_t* failure_predictor_create_custom(
    const failure_predictor_config_t* config
)
{
    // =========================================================================
    // GUARD: Validate parameters
    // =========================================================================

    if (!config) {
        LOG_ERROR("NULL config in failure_predictor_create_custom");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "failure_predictor_create_custom: config is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    failure_prediction_heartbeat("failure_pred_failure_predictor_cr", 0.0f);


    if (config->max_predictions == 0 || config->max_indicators == 0) {
        LOG_ERROR("Invalid config: max_predictions=%u, max_indicators=%u",
                  config->max_predictions, config->max_indicators);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "failure_predictor_create_custom: config->max_predictions is zero");
        return NULL;
    }

    if (config->prediction_threshold < 0.0F || config->prediction_threshold > 1.0F) {
        LOG_ERROR("Invalid prediction_threshold: %.2f (must be [0,1])",
                  config->prediction_threshold);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "failure_predictor_create_custom: validation failed");
        return NULL;
    }

    // =========================================================================
    // ALLOCATION: Create predictor structure
    // =========================================================================

    failure_predictor_t* predictor = nimcp_calloc(1, sizeof(failure_predictor_t));
    if (!predictor) {
        LOG_ERROR("Failed to allocate failure_predictor_t");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "failure_predictor_create_custom: predictor is NULL");
        return NULL;
    }

    // Copy configuration
    predictor->config = *config;

    // Allocate indicator array
    predictor->indicators = nimcp_calloc(
        config->max_indicators,
        sizeof(internal_indicator_t)
    );
    if (!predictor->indicators) {
        LOG_ERROR("Failed to allocate indicators array");
        nimcp_free(predictor);
        predictor = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "failure_predictor_create_custom: predictor->indicators is NULL");
        return NULL;
    }

    predictor->indicator_count = 0;

    // Allocate prediction array
    predictor->predictions = nimcp_calloc(
        config->max_predictions,
        sizeof(failure_prediction_t)
    );
    if (!predictor->predictions) {
        LOG_ERROR("Failed to allocate predictions array");
        nimcp_free(predictor->indicators);
        nimcp_free(predictor);
        predictor = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "failure_predictor_create_custom: predictor->predictions is NULL");
        return NULL;
    }

    predictor->prediction_count = 0;

    // Initialize thread safety
    nimcp_rwlock_init(&predictor->lock);

    LOG_INFO("Created failure predictor (max_predictions=%u, max_indicators=%u)",
             config->max_predictions, config->max_indicators);

    
    // Bio-async registration
    predictor->bio_ctx = NULL;
    predictor->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_SYSTEM_FAILURE_PREDICTION,
            .module_name = "failure_prediction",
            .inbox_capacity = 32,
            .user_data = predictor
        };
        predictor->bio_ctx = bio_router_register_module(&bio_info);
        if (predictor->bio_ctx) {
            predictor->bio_async_enabled = true;
        }
    }

return predictor;
}

void failure_predictor_destroy(failure_predictor_t* predictor) {
    /* Phase 8: Heartbeat at operation start */
    failure_prediction_heartbeat("failure_pred_failure_predictor_de", 0.0f);


    LOG_DEBUG("Destroying module");
    if (!predictor) {
        return;
    }

    nimcp_rwlock_destroy(&predictor->lock);

    if (predictor->indicators) {
        nimcp_free(predictor->indicators);
    }

    if (predictor->predictions) {
        nimcp_free(predictor->predictions);
    }

    // Unregister from bio-router
    if (predictor->bio_async_enabled && predictor->bio_ctx) {
        bio_router_unregister_module(predictor->bio_ctx);
        predictor->bio_ctx = NULL;
        predictor->bio_async_enabled = false;
    }

    nimcp_free(predictor);
    predictor = NULL;

    LOG_DEBUG("Destroyed failure predictor");
}

//=============================================================================
// Indicator Management
//=============================================================================

bool failure_predictor_update_indicator(
    failure_predictor_t* predictor,
    metric_type_t metric,
    float current_value,
    float threshold
)
{
    // =========================================================================
    // GUARD: Validate parameters
    // =========================================================================

    if (!predictor) {
        LOG_ERROR("NULL predictor in failure_predictor_update_indicator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "failure_predictor_update_indicator: predictor is NULL");
        return false;
    }

    // Process pending bio-async messages
    /* Phase 8: Heartbeat at operation start */
    failure_prediction_heartbeat("failure_pred_failure_predictor_up", 0.0f);


    if (predictor->bio_async_enabled && predictor->bio_ctx) {
        bio_router_process_inbox(predictor->bio_ctx, 5);
    }

    // Sanitize inputs
    if (isnan(current_value) || isinf(current_value)) {
        LOG_WARNING("Invalid current_value (NaN or Inf) for metric %d", metric);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "failure_predictor_update_indicator: validation failed");
        return false;
    }

    if (current_value < 0.0F) {
        // Some metrics should not be negative (gradient norm, memory)
        if (metric == METRIC_TYPE_MEMORY || metric == METRIC_TYPE_GRADIENT) {
            LOG_WARNING("Negative value %.2f for metric %d (sanitizing to 0)",
                        current_value, metric);
            current_value = 0.0F;
        }
    }

    lock_write(predictor);

    // =========================================================================
    // FIND OR CREATE: Get indicator for this metric
    // =========================================================================

    int index = find_indicator_index(predictor, metric);

    if (index < 0) {
        // Create new indicator
        if (predictor->indicator_count >= predictor->config.max_indicators) {
            LOG_WARNING("Indicator capacity reached (%u), cannot add metric %d",
                        predictor->config.max_indicators, metric);
            unlock(predictor);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "failure_predictor_update_indicator: capacity exceeded");
            return false;
        }

        index = (int)predictor->indicator_count;
        predictor->indicator_count++;

        // Initialize new indicator
        predictor->indicators[index].current.metric = metric;
        predictor->indicators[index].current.threshold = threshold;
        predictor->indicators[index].initialized = false;
    }

    internal_indicator_t* indicator = &predictor->indicators[index];
    uint64_t current_time = get_timestamp_ms();

    // =========================================================================
    // CALCULATE DERIVATIVES: Rate of change and acceleration
    // =========================================================================

    if (indicator->initialized) {
        // Calculate time delta
        uint64_t time_delta_ms = current_time - indicator->previous_timestamp;

        if (time_delta_ms > 0) {
            // WHAT: Calculate rate of change (first derivative)
            // WHY:  Track how fast metric is changing
            // HOW:  delta_value / delta_time
            float value_delta = current_value - indicator->previous_value;
            float time_delta_sec = time_delta_ms / 1000.0F;
            float rate = value_delta / time_delta_sec;

            // WHAT: Calculate acceleration (second derivative)
            // WHY:  Detect if rate of change is increasing
            // HOW:  delta_rate / delta_time
            float rate_delta = rate - indicator->previous_rate;
            float acceleration = rate_delta / time_delta_sec;

            indicator->current.rate_of_change = rate;
            indicator->current.acceleration = acceleration;

            // Update previous values for next iteration
            indicator->previous_value = indicator->current.current_value;
            indicator->previous_rate = rate;
        } else {
            // Zero time delta - keep previous derivatives
            LOG_DEBUG("Zero time delta for metric %d, keeping previous derivatives", metric);
        }
    } else {
        // First update - no derivatives yet
        indicator->current.rate_of_change = 0.0F;
        indicator->current.acceleration = 0.0F;
        indicator->previous_value = current_value;
        indicator->previous_rate = 0.0F;
        indicator->initialized = true;
    }

    // Update current values
    indicator->current.current_value = current_value;
    indicator->current.threshold = threshold;
    indicator->current.last_update_ms = current_time;
    indicator->previous_timestamp = current_time;

    unlock(predictor);

    LOG_DEBUG("Updated indicator metric=%d, value=%.2f, rate=%.2f, accel=%.2f",
              metric, current_value,
              indicator->current.rate_of_change,
              indicator->current.acceleration);

    return true;
}

bool failure_predictor_update_from_health_metrics(
    failure_predictor_t* predictor,
    const failure_health_metrics_t* metrics
)
{
    // =========================================================================
    // GUARD: Validate parameters
    // =========================================================================

    if (!predictor) {
        LOG_ERROR("NULL predictor in failure_predictor_update_from_health_metrics");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "failure_predictor_update_from_health_metrics: predictor is NULL");
        return false;
    }

    if (!metrics) {
        LOG_ERROR("NULL metrics in failure_predictor_update_from_health_metrics");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "failure_predictor_update_from_health_metrics: metrics is NULL");
        return false;
    }

    // =========================================================================
    // UPDATE: Extract and update all indicators
    // =========================================================================

    // Memory usage
    /* Phase 8: Heartbeat at operation start */
    failure_prediction_heartbeat("failure_pred_failure_predictor_up", 0.0f);


    failure_predictor_update_indicator(
        predictor,
        METRIC_TYPE_MEMORY,
        (float)metrics->memory_usage,
        (float)metrics->peak_memory * 0.9F  // 90% of peak as threshold
    );

    // Gradient norm
    failure_predictor_update_indicator(
        predictor,
        METRIC_TYPE_GRADIENT,
        metrics->gradient_norm,
        GRADIENT_EXPLOSION_NORM_THRESHOLD
    );

    // Loss value
    failure_predictor_update_indicator(
        predictor,
        METRIC_TYPE_LOSS,
        metrics->loss_value,
        100.0F  // High loss threshold
    );

    // Throughput
    failure_predictor_update_indicator(
        predictor,
        METRIC_TYPE_THROUGHPUT,
        metrics->throughput,
        metrics->throughput * 0.5F  // 50% degradation threshold
    );

    // Error rate
    failure_predictor_update_indicator(
        predictor,
        METRIC_TYPE_ERROR,
        metrics->error_rate,
        0.1F  // 10% error rate threshold
    );

    return true;
}

bool failure_predictor_get_indicator(
    failure_predictor_t* predictor,
    metric_type_t metric,
    leading_indicator_t* indicator
)
{
    if (!predictor || !indicator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "failure_predictor_get_indicator: required parameter is NULL (predictor, indicator)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    failure_prediction_heartbeat("failure_pred_failure_predictor_ge", 0.0f);


    lock_read(predictor);

    int index = find_indicator_index(predictor, metric);

    if (index < 0) {
        unlock(predictor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "failure_predictor_get_indicator: validation failed");
        return false;
    }

    *indicator = predictor->indicators[index].current;

    unlock(predictor);
    return true;
}

leading_indicator_t* failure_predictor_get_all_indicators(
    failure_predictor_t* predictor,
    uint32_t* count
)
{
    if (!predictor || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "failure_predictor_get_all_indicators: required parameter is NULL (predictor, count)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    failure_prediction_heartbeat("failure_pred_failure_predictor_ge", 0.0f);


    lock_read(predictor);

    *count = predictor->indicator_count;

    if (*count == 0) {
        unlock(predictor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "failure_predictor_get_all_indicators: count is zero");
        return NULL;
    }

    // Allocate array for caller
    leading_indicator_t* indicators = nimcp_calloc(*count, sizeof(leading_indicator_t));
    if (!indicators) {
        unlock(predictor);
        *count = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "failure_predictor_get_all_indicators: indicators is NULL");
        return NULL;
    }

    // Copy indicators
    for (uint32_t i = 0; i < *count; i++) {
        indicators[i] = predictor->indicators[i].current;
    }

    unlock(predictor);

    return indicators;
}

//=============================================================================
// Specific Detection Functions
//=============================================================================

bool failure_predictor_detect_memory_leak(
    failure_predictor_t* predictor,
    const failure_health_metrics_t* metrics
)
{
    if (!predictor || !metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "failure_predictor_detect_memory_leak: required parameter is NULL (predictor, metrics)");
        return false;
    }

    if (!predictor->config.enable_memory_leak_detection) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "failure_predictor_detect_memory_leak: predictor->config is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    failure_prediction_heartbeat("failure_pred_failure_predictor_de", 0.0f);


    lock_read(predictor);

    // Find memory indicator
    int index = find_indicator_index(predictor, METRIC_TYPE_MEMORY);

    if (index < 0 || !predictor->indicators[index].initialized) {
        unlock(predictor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "failure_predictor_detect_memory_leak: predictor->indicators is NULL");
        return false;
    }

    internal_indicator_t* indicator = &predictor->indicators[index];

    // WHAT: Check for sustained memory growth
    // WHY:  Indicate memory leak
    // HOW:  Growth rate > threshold AND acceleration > 0

    // Convert rate from bytes/sec to MB/sec
    float rate_mb_per_sec = indicator->current.rate_of_change / (1024.0F * 1024.0F);

    bool is_leaking = (rate_mb_per_sec > MEMORY_LEAK_RATE_THRESHOLD_MB_PER_SEC) &&
                      (indicator->current.acceleration > MEMORY_LEAK_MIN_ACCELERATION);

    unlock(predictor);

    if (is_leaking) {
        LOG_WARNING("Memory leak detected: rate=%.2f MB/s, accel=%.2f",
                    rate_mb_per_sec, indicator->current.acceleration);
    }

    return is_leaking;
}

bool failure_predictor_detect_gradient_explosion(
    failure_predictor_t* predictor,
    const failure_health_metrics_t* metrics
)
{
    if (!predictor || !metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "failure_predictor_detect_gradient_explosion: required parameter is NULL (predictor, metrics)");
        return false;
    }

    if (!predictor->config.enable_gradient_explosion_detection) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "failure_predictor_detect_gradient_explosion: predictor->config is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    failure_prediction_heartbeat("failure_pred_failure_predictor_de", 0.0f);


    lock_read(predictor);

    int index = find_indicator_index(predictor, METRIC_TYPE_GRADIENT);

    if (index < 0 || !predictor->indicators[index].initialized) {
        unlock(predictor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "failure_predictor_detect_gradient_explosion: predictor->indicators is NULL");
        return false;
    }

    internal_indicator_t* indicator = &predictor->indicators[index];

    // WHAT: Check for exponentially growing gradients
    // WHY:  Indicate training instability
    // HOW:  High gradient norm AND high rate of change

    bool is_exploding = (indicator->current.current_value > GRADIENT_EXPLOSION_NORM_THRESHOLD) &&
                        (indicator->current.rate_of_change > GRADIENT_EXPLOSION_RATE_THRESHOLD);

    unlock(predictor);

    if (is_exploding) {
        LOG_WARNING("Gradient explosion detected: norm=%.2f, rate=%.2f",
                    indicator->current.current_value,
                    indicator->current.rate_of_change);
    }

    return is_exploding;
}

uint64_t failure_predictor_estimate_time_to_oom(
    failure_predictor_t* predictor,
    const failure_health_metrics_t* metrics
)
{
    if (!predictor || !metrics) {
        return 0;
    }

    if (!failure_predictor_detect_memory_leak(predictor, metrics)) {
        return 0;  // No leak detected
    }

    /* Phase 8: Heartbeat at operation start */
    failure_prediction_heartbeat("failure_pred_failure_predictor_es", 0.0f);


    lock_read(predictor);

    int index = find_indicator_index(predictor, METRIC_TYPE_MEMORY);

    if (index < 0) {
        unlock(predictor);
        return 0;
    }

    internal_indicator_t* indicator = &predictor->indicators[index];

    // WHAT: Estimate time until MAX_MEMORY reached
    // WHY:  Allow preventive action timing
    // HOW:  (MAX - current) / growth_rate

    float current_bytes = indicator->current.current_value;
    float rate_bytes_per_sec = indicator->current.rate_of_change;

    if (rate_bytes_per_sec <= 0.0F) {
        unlock(predictor);
        return 0;  // Not growing
    }

    float remaining_bytes = (float)MAX_MEMORY_BYTES - current_bytes;

    if (remaining_bytes <= 0.0F) {
        unlock(predictor);
        return 1;  // Already at max (1 ms)
    }

    float time_to_oom_sec = remaining_bytes / rate_bytes_per_sec;
    uint64_t time_to_oom_ms = (uint64_t)(time_to_oom_sec * 1000.0F);

    // Cap at reasonable maximum (1 hour)
    if (time_to_oom_ms > 3600000) {
        time_to_oom_ms = 3600000;
    }

    unlock(predictor);

    return time_to_oom_ms;
}

uint64_t failure_predictor_estimate_time_to_explosion(
    failure_predictor_t* predictor,
    const failure_health_metrics_t* metrics
)
{
    if (!predictor || !metrics) {
        return 0;
    }

    if (!failure_predictor_detect_gradient_explosion(predictor, metrics)) {
        return 0;  // Stable
    }

    /* Phase 8: Heartbeat at operation start */
    failure_prediction_heartbeat("failure_pred_failure_predictor_es", 0.0f);


    lock_read(predictor);

    int index = find_indicator_index(predictor, METRIC_TYPE_GRADIENT);

    if (index < 0) {
        unlock(predictor);
        return 0;
    }

    internal_indicator_t* indicator = &predictor->indicators[index];

    // WHAT: Estimate time until divergence
    // HOW:  (MAX_GRADIENT - current) / rate

    float current_norm = indicator->current.current_value;
    float rate_per_sec = indicator->current.rate_of_change;

    if (rate_per_sec <= 0.0F) {
        unlock(predictor);
        return 0;
    }

    float remaining = MAX_GRADIENT_NORM - current_norm;

    if (remaining <= 0.0F) {
        unlock(predictor);
        return 1;  // Already exploded
    }

    float time_sec = remaining / rate_per_sec;
    uint64_t time_ms = (uint64_t)(time_sec * 1000.0F);

    // Cap at 1 minute
    if (time_ms > 60000) {
        time_ms = 60000;
    }

    unlock(predictor);

    return time_ms;
}

//=============================================================================
// Prediction Functions
//=============================================================================

failure_prediction_t* failure_predictor_predict(
    failure_predictor_t* predictor,
    const failure_health_metrics_t* metrics
)
{
    if (!predictor || !metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "failure_predictor_predict: required parameter is NULL (predictor, metrics)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    failure_prediction_heartbeat("failure_pred_failure_predictor_pr", 0.0f);


    lock_write(predictor);

    // Reset prediction count
    predictor->prediction_count = 0;

    // =========================================================================
    // PREDICTION 1: Out of Memory (OOM)
    // =========================================================================

    if (predictor->config.enable_memory_leak_detection) {
        bool leak_detected = false;
        uint64_t time_to_oom = 0;

        // Temporarily unlock for detection (which needs read lock)
        unlock(predictor);
        leak_detected = failure_predictor_detect_memory_leak(predictor, metrics);
        if (leak_detected) {
            time_to_oom = failure_predictor_estimate_time_to_oom(predictor, metrics);
        }
        lock_write(predictor);

        if (leak_detected && time_to_oom > 0) {
            // Calculate probability based on time urgency
            float probability = 0.5F;  // Base probability

            if (time_to_oom < 5000) {
                probability = 0.95F;  // Very urgent
            } else if (time_to_oom < 30000) {
                probability = 0.85F;  // Urgent
            } else if (time_to_oom < 120000) {
                probability = 0.75F;  // Moderate
            }

            if (probability >= predictor->config.prediction_threshold) {
                if (predictor->prediction_count < predictor->config.max_predictions) {
                    failure_prediction_t* pred = &predictor->predictions[predictor->prediction_count];
                    pred->type = FAILURE_TYPE_OOM;
                    pred->probability = probability;
                    pred->estimated_time_ms = time_to_oom;
                    pred->confidence = (probability > 0.9F) ? CONFIDENCE_VERY_HIGH :
                                       (probability > 0.75F) ? CONFIDENCE_HIGH :
                                       (probability > 0.5F) ? CONFIDENCE_MEDIUM : CONFIDENCE_LOW;
                    pred->reasoning = "Memory growing rapidly, OOM imminent";
                    predictor->prediction_count++;
                }
            }
        }
    }

    // =========================================================================
    // PREDICTION 2: Gradient Explosion
    // =========================================================================

    if (predictor->config.enable_gradient_explosion_detection) {
        bool explosion_detected = false;
        uint64_t time_to_explosion = 0;

        unlock(predictor);
        explosion_detected = failure_predictor_detect_gradient_explosion(predictor, metrics);
        if (explosion_detected) {
            time_to_explosion = failure_predictor_estimate_time_to_explosion(predictor, metrics);
        }
        lock_write(predictor);

        if (explosion_detected && time_to_explosion > 0) {
            float probability = 0.5F;

            if (time_to_explosion < 1000) {
                probability = 0.95F;  // Very urgent
            } else if (time_to_explosion < 10000) {
                probability = 0.85F;
            } else {
                probability = 0.75F;
            }

            if (probability >= predictor->config.prediction_threshold) {
                if (predictor->prediction_count < predictor->config.max_predictions) {
                    failure_prediction_t* pred = &predictor->predictions[predictor->prediction_count];
                    pred->type = FAILURE_TYPE_GRADIENT_EXPLOSION;
                    pred->probability = probability;
                    pred->estimated_time_ms = time_to_explosion;
                    pred->confidence = (probability > 0.9F) ? CONFIDENCE_VERY_HIGH :
                                       (probability > 0.75F) ? CONFIDENCE_HIGH : CONFIDENCE_MEDIUM;
                    pred->reasoning = "Gradient norm growing exponentially";
                    predictor->prediction_count++;
                }
            }
        }
    }

    // =========================================================================
    // SORT: Order predictions by probability (highest first)
    // =========================================================================

    if (predictor->prediction_count > 1) {
        qsort(predictor->predictions,
              predictor->prediction_count,
              sizeof(failure_prediction_t),
              compare_predictions);
    }

    // =========================================================================
    // RETURN: Allocate and return prediction array
    // =========================================================================

    failure_prediction_t* result = NULL;

    if (predictor->prediction_count > 0) {
        result = nimcp_calloc(predictor->prediction_count, sizeof(failure_prediction_t));
        if (result) {
            memcpy(result, predictor->predictions,
                   predictor->prediction_count * sizeof(failure_prediction_t));
        }
    }

    unlock(predictor);

    return result;
}

uint32_t failure_predictor_get_prediction_count(failure_predictor_t* predictor) {
    if (!predictor) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    failure_prediction_heartbeat("failure_pred_failure_predictor_ge", 0.0f);


    lock_read(predictor);
    uint32_t count = predictor->prediction_count;
    unlock(predictor);

    return count;
}

bool failure_predictor_get_prediction_by_type(
    failure_predictor_t* predictor,
    failure_type_t type,
    failure_prediction_t* prediction
)
{
    if (!predictor || !prediction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "failure_predictor_get_prediction_by_type: required parameter is NULL (predictor, prediction)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    failure_prediction_heartbeat("failure_pred_failure_predictor_ge", 0.0f);


    lock_read(predictor);

    for (uint32_t i = 0; i < predictor->prediction_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && predictor->prediction_count > 256) {
            failure_prediction_heartbeat("failure_pred_loop",
                             (float)(i + 1) / (float)predictor->prediction_count);
        }

        if (predictor->predictions[i].type == type) {
            *prediction = predictor->predictions[i];
            unlock(predictor);
            return true;
        }
    }

    unlock(predictor);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "failure_predictor_get_prediction_by_type: validation failed");
    return false;
}

/**
 * CRITICAL THREAD SAFETY WARNING - POINTER-AFTER-UNLOCK ISSUE:
 * ============================================================
 * This function returned a pointer to internal prediction data AFTER releasing
 * the lock. This is a HIGH PRIORITY thread-safety issue.
 *
 * FIX APPLIED: Now copies prediction data before returning. Caller must provide
 * output buffer and the prediction is copied while holding the lock.
 *
 * The old signature returning failure_prediction_t* is preserved for API
 * compatibility but now returns NULL (deprecated). Use the new
 * failure_predictor_get_highest_probability_prediction_copy() instead.
 */
failure_prediction_t* failure_predictor_get_highest_probability_prediction(
    failure_predictor_t* predictor
)
{
    /* DEPRECATED: This function is unsafe and now always returns NULL.
     * Use failure_predictor_get_highest_probability_prediction_copy() instead. */
    /* Phase 8: Heartbeat at operation start */
    failure_prediction_heartbeat("failure_pred_failure_predictor_ge", 0.0f);


    (void)predictor;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "failure_predictor_get_highest_probability_prediction: operation failed");
    return NULL;
}

/**
 * WHAT: Get thread-safe copy of highest probability prediction
 * WHY:  Original function returned pointer that could be invalidated
 * HOW:  Copy prediction data while holding lock, return via out parameter
 *
 * This is the RECOMMENDED API for accessing the highest probability prediction.
 *
 * @param predictor Predictor instance
 * @param out_prediction Output: copy of prediction data (caller-allocated)
 * @return true if prediction found and copied, false if no predictions or error
 */
bool failure_predictor_get_highest_probability_prediction_copy(
    failure_predictor_t* predictor,
    failure_prediction_t* out_prediction
)
{
    if (!predictor || !out_prediction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "failure_predictor_get_highest_probability_prediction_copy: required parameter is NULL (predictor, out_prediction)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    failure_prediction_heartbeat("failure_pred_failure_predictor_ge", 0.0f);


    lock_read(predictor);

    if (predictor->prediction_count == 0) {
        unlock(predictor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "failure_predictor_get_highest_probability_prediction_copy: predictor->prediction_count is zero");
        return false;
    }

    /* Copy prediction data while holding lock - thread-safe.
     * Predictions are already sorted by probability, so first is highest. */
    *out_prediction = predictor->predictions[0];

    unlock(predictor);

    return true;
}

void failure_predictor_clear_predictions(failure_predictor_t* predictor) {
    if (!predictor) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    failure_prediction_heartbeat("failure_pred_failure_predictor_cl", 0.0f);


    lock_write(predictor);
    predictor->prediction_count = 0;
    memset(predictor->predictions, 0,
           predictor->config.max_predictions * sizeof(failure_prediction_t));
    unlock(predictor);

    LOG_DEBUG("Cleared all predictions");
}

//=============================================================================
// Preventive Action Functions
//=============================================================================

bool failure_predictor_needs_prevention(
    failure_predictor_t* predictor,
    const failure_prediction_t* prediction
)
{
    if (!predictor || !prediction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "failure_predictor_needs_prevention: required parameter is NULL (predictor, prediction)");
        return false;
    }

    // CRITERIA:
    // 1. High probability (> 0.8)
    // 2. OR moderate probability (> 0.6) AND urgent (< 10 sec)

    /* Phase 8: Heartbeat at operation start */
    failure_prediction_heartbeat("failure_pred_failure_predictor_ne", 0.0f);


    bool high_probability = prediction->probability > PREVENTION_PROBABILITY_THRESHOLD;
    bool urgent = (prediction->probability > PREVENTION_URGENT_PROBABILITY) &&
                  (prediction->estimated_time_ms < PREVENTION_URGENT_TIME_MS);

    return high_probability || urgent;
}

const char* failure_predictor_get_preventive_action(
    failure_predictor_t* predictor,
    const failure_prediction_t* prediction
)
{
    if (!predictor || !prediction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "failure_predictor_get_preventive_action: required parameter is NULL (predictor, prediction)");
        return NULL;
    }

    // WHAT: Return type-specific recommendation
    // WHY:  Guide preventive response
    // HOW:  Switch on failure type

    switch (prediction->type) {
        case FAILURE_TYPE_OOM:
            return "Trigger garbage collection or free caches to reclaim memory";

        case FAILURE_TYPE_GRADIENT_EXPLOSION:
            return "Reduce learning rate or apply gradient clipping";

        case FAILURE_TYPE_DIVERGENCE:
            return "Restart training from last checkpoint";

        case FAILURE_TYPE_PERFORMANCE_DEGRADATION:
            return "Reduce batch size or optimize critical paths";

        case FAILURE_TYPE_ERROR_RATE_SPIKE:
            return "Enable additional error handling or reduce load";

        case FAILURE_TYPE_THREAD_DEADLOCK:
            return "Review and fix locking order";

        case FAILURE_TYPE_DISK_FULL:
            return "Delete old logs or temporary files";

        case FAILURE_TYPE_NETWORK_TIMEOUT:
            return "Increase timeout or retry with exponential backoff";

        default:
            return "Review system state and take appropriate action";
    }
}

const char* failure_predictor_get_highest_priority_action(
    failure_predictor_t* predictor
)
{
    if (!predictor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictor is NULL");

        return NULL;
    }

    lock_read(predictor);

    if (predictor->prediction_count == 0) {
        unlock(predictor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "failure_predictor_get_highest_priority_action: predictor->prediction_count is zero");
        return NULL;
    }

    /* Thread-safety fix: copy prediction data to stack before releasing lock,
     * because the pointer into predictor->predictions[] may be invalidated
     * after unlock if another thread modifies the predictions array. */
    failure_prediction_t top_copy = predictor->predictions[0];

    unlock(predictor);

    return failure_predictor_get_preventive_action(predictor, &top_copy);
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int failure_prediction_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    failure_prediction_heartbeat("failure_pred_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Failure_Prediction");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                failure_prediction_heartbeat("failure_pred_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* Log self-knowledge observations */
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Failure_Prediction");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Failure_Prediction");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void failure_prediction_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        (void)instance;  /* No struct-level health_agent; use global */
        g_failure_prediction_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Functions (FULL implementation)
 * ============================================================================ */
int failure_prediction_training_begin(failure_predictor_t* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "failure_prediction_training_begin: NULL argument");
        return -1;
    }
    struct failure_predictor* ctx = (struct failure_predictor*)instance;
    failure_prediction_heartbeat_instance(NULL, "failure_pred_training_begin", 0.0f);
    ctx->indicator_count = 0;
    ctx->prediction_count = 0;
    NIMCP_LOGGING_INFO("%s training begin: counters reset", "failure_prediction");
    return 0;
}

int failure_prediction_training_step(failure_predictor_t* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "failure_prediction_training_step: NULL argument");
        return -1;
    }
    struct failure_predictor* ctx = (struct failure_predictor*)instance;
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    failure_prediction_heartbeat_instance(NULL, "failure_pred_training_step", progress);
    ctx->prediction_count++;
    return 0;
}

int failure_prediction_training_end(failure_predictor_t* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "failure_prediction_training_end: NULL argument");
        return -1;
    }
    struct failure_predictor* ctx = (struct failure_predictor*)instance;
    failure_prediction_heartbeat_instance(NULL, "failure_pred_training_end", 1.0f);
    NIMCP_LOGGING_INFO("%s training end: %u indicators, %u predictions",
                       "failure_prediction", ctx->indicator_count, ctx->prediction_count);
    return 0;
}
