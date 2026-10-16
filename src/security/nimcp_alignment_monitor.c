/**
 * @file nimcp_alignment_monitor.c
 * @brief Alignment Drift Monitor Implementation
 * @version 1.0.0
 * @date 2026-02-01
 *
 * WHAT: Implementation of alignment drift monitoring
 * WHY:  Detect value drift before it becomes dangerous
 * HOW:  Statistical divergence measures, running statistics
 */

#include "security/nimcp_alignment_monitor.h"
#include "constants/nimcp_constants.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/statistics/nimcp_statistics.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define LOG_CATEGORY "alignment_monitor"

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

/* Forward declaration for health agent */
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "utils/exception/nimcp_exception_macros.h"

BRIDGE_BOILERPLATE_MESH_ONLY(alignment, MESH_ADAPTER_CATEGORY_SECURITY)


/* Default value dimension names */
static const char* DEFAULT_VALUE_NAMES[] = {
    "helpfulness",
    "honesty",
    "harmlessness",
    "fairness",
    "autonomy_respect",
    "privacy",
    "transparency",
    "reliability",
    "safety",
    "corrigibility",
    "beneficence",
    "non_maleficence",
    "justice",
    "dignity",
    "freedom",
    "wellbeing"
};

#define DEFAULT_VALUE_COUNT (sizeof(DEFAULT_VALUE_NAMES) / sizeof(DEFAULT_VALUE_NAMES[0]))

/* ============================================================================
 * Internal Types
 * ============================================================================ */

/**
 * @brief Observation circular buffer
 */
typedef struct observation_buffer {
    alignment_action_observation_t* actions;
    size_t action_capacity;
    size_t action_count;
    size_t action_index;

    alignment_explanation_observation_t* explanations;
    size_t explanation_capacity;
    size_t explanation_count;
    size_t explanation_index;
} observation_buffer_t;

/**
 * @brief Alignment monitor internal state
 */
struct alignment_monitor {
    uint32_t magic;
    nimcp_mutex_t* mutex;

    /* Configuration */
    alignment_monitor_config_t config;

    /* Baseline (immutable after init) */
    float baseline_values[ALIGNMENT_VALUE_DIMENSIONS];
    uint8_t baseline_hash[32];
    uint64_t baseline_timestamp;

    /* Current state */
    float current_values[ALIGNMENT_VALUE_DIMENSIONS];
    value_running_stats_t value_stats[ALIGNMENT_VALUE_DIMENSIONS];

    /* Observation history */
    observation_buffer_t observations;

    /* Drift event history */
    alignment_drift_event_t drift_events[ALIGNMENT_MAX_DRIFT_EVENTS];
    size_t drift_event_count;
    size_t drift_event_index;

    /* Statistics */
    alignment_monitor_stats_t stats;
    uint64_t start_time;

    /* Integration handles */
    void* tripwires;
    void* value_commitment;
    void* brain_immune;              /**< Brain immune system for drift antigen presentation */

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_connected;

    /* Cached status */
    alignment_status_t cached_status;
    uint64_t cached_status_time;
    bool cached_status_valid;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Validate alignment monitor handle
 */
static bool is_valid_handle(const alignment_monitor_t* monitor)
{
    return monitor != NULL && monitor->magic == ALIGNMENT_MONITOR_MAGIC;
}

/**
 * @brief Get current time in microseconds
 */
static uint64_t get_time_us(void)
{
    return nimcp_time_now_us();
}

/**
 * @brief Copy string safely with null termination
 */
static void safe_strcpy(char* dest, const char* src, size_t max_len)
{
    if (dest == NULL || max_len == 0) {
        return;
    }
    if (src == NULL) {
        dest[0] = '\0';
        return;
    }
    size_t len = strlen(src);
    if (len >= max_len) {
        len = max_len - 1;
    }
    memcpy(dest, src, len);
    dest[len] = '\0';
}

/**
 * @brief Initialize running stats for a value
 */
static void init_value_stats(value_running_stats_t* stats)
{
    stats->n = 0;
    stats->mean = 0.0;
    stats->m2 = 0.0;
    stats->min = INFINITY;
    stats->max = -INFINITY;
}

/**
 * @brief Update running stats (Welford's algorithm)
 */
static void update_value_stats(value_running_stats_t* stats, double x)
{
    stats->n++;
    double delta = x - stats->mean;
    stats->mean += delta / (double)stats->n;
    double delta2 = x - stats->mean;
    stats->m2 += delta * delta2;

    if (x < stats->min) stats->min = x;
    if (x > stats->max) stats->max = x;
}

/**
 * @brief Get variance from running stats
 */
static double get_stats_variance(const value_running_stats_t* stats)
{
    if (stats->n < 2) return 0.0;
    return stats->m2 / (double)(stats->n - 1);
}

/**
 * @brief Compute cosine similarity between two vectors
 */
static float compute_cosine_similarity(
    const float* a,
    const float* b,
    size_t n)
{
    float dot = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;

    for (size_t i = 0; i < n; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    float denom = sqrtf(norm_a) * sqrtf(norm_b);
    if (denom < 1e-10f) return 0.0f;

    return dot / denom;
}

/**
 * @brief Compute Euclidean distance between two vectors
 */
static float compute_euclidean_distance(
    const float* a,
    const float* b,
    size_t n)
{
    float sum = 0.0f;
    for (size_t i = 0; i < n; i++) {
        float diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sqrtf(sum);
}

/**
 * @brief Normalize values to probability distribution
 */
static void normalize_to_distribution(
    const float* values,
    float* probs,
    size_t n)
{
    float sum = 0.0f;
    for (size_t i = 0; i < n; i++) {
        /* Ensure non-negative for probability */
        probs[i] = fabsf(values[i]) + 1e-10f;
        sum += probs[i];
    }
    for (size_t i = 0; i < n; i++) {
        probs[i] /= sum;
    }
}

/**
 * @brief Add drift event to history
 */
static void add_drift_event(
    alignment_monitor_t* monitor,
    const alignment_drift_event_t* event)
{
    size_t idx = monitor->drift_event_index;
    memcpy(&monitor->drift_events[idx], event, sizeof(*event));
    monitor->drift_event_index = (idx + 1) % ALIGNMENT_MAX_DRIFT_EVENTS;
    if (monitor->drift_event_count < ALIGNMENT_MAX_DRIFT_EVENTS) {
        monitor->drift_event_count++;
    }
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

alignment_monitor_config_t alignment_monitor_default_config(void)
{
    alignment_monitor_config_t config;
    memset(&config, 0, sizeof(config));

    /* Initialize value dimensions with defaults */
    config.active_dimensions = ALIGNMENT_VALUE_DIMENSIONS;
    for (uint32_t i = 0; i < ALIGNMENT_VALUE_DIMENSIONS; i++) {
        if (i < DEFAULT_VALUE_COUNT) {
            safe_strcpy(config.value_dimensions[i].name,
                       DEFAULT_VALUE_NAMES[i],
                       ALIGNMENT_VALUE_NAME_MAX_LENGTH);
        }
        config.value_dimensions[i].weight = 1.0f;
        config.value_dimensions[i].baseline_value = 1.0f;
        config.value_dimensions[i].current_value = 1.0f;
        config.value_dimensions[i].min_acceptable = 0.5f;
        config.value_dimensions[i].max_acceptable = 1.0f;
    }

    /* Set thresholds */
    config.thresholds.kl_divergence_threshold = 0.5f;
    config.thresholds.js_divergence_threshold = 0.3f;
    config.thresholds.mutual_info_threshold = 0.1f;
    config.thresholds.cosine_similarity_min = 0.9f;
    config.thresholds.euclidean_distance_max = 0.5f;
    config.thresholds.max_single_dimension_drift = 0.2f;
    config.thresholds.action_consistency_min = 0.8f;
    config.thresholds.explanation_consistency_min = 0.8f;
    config.thresholds.posterior_stability_threshold = 0.1f;
    config.thresholds.observation_window_size = 1000;
    config.thresholds.enable_exponential_weighting = true;
    config.thresholds.exponential_decay_rate = NIMCP_EMA_DECAY_DEFAULT;

    /* Monitoring settings */
    config.enable_continuous_monitoring = true;
    config.update_interval_ms = 1000;
    config.enable_bayesian_inference = true;
    config.enable_exponential_smoothing = true;
    config.smoothing_alpha = 0.1f;

    /* Integration */
    config.alert_on_drift = true;
    config.connect_to_tripwires = true;

    return config;
}

alignment_monitor_t* alignment_monitor_create(
    const alignment_monitor_config_t* config)
{
    alignment_monitor_t* monitor = nimcp_calloc(1, sizeof(alignment_monitor_t));
    if (monitor == NULL) {
        NIMCP_LOG_ERROR(LOG_CATEGORY, "Failed to allocate alignment monitor");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "alignment_monitor_create: validation failed");
        return NULL;
    }

    /* Initialize mutex */
    monitor->mutex = nimcp_mutex_create(NULL);
    if (monitor->mutex == NULL) {
        NIMCP_LOG_ERROR(LOG_CATEGORY, "Failed to create mutex");
        nimcp_free(monitor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "alignment_monitor_create: validation failed");
        return NULL;
    }

    /* Apply configuration */
    if (config != NULL) {
        memcpy(&monitor->config, config, sizeof(*config));
    } else {
        monitor->config = alignment_monitor_default_config();
    }

    /* Initialize baseline values */
    for (uint32_t i = 0; i < ALIGNMENT_VALUE_DIMENSIONS; i++) {
        monitor->baseline_values[i] = monitor->config.value_dimensions[i].baseline_value;
        monitor->current_values[i] = monitor->baseline_values[i];
        init_value_stats(&monitor->value_stats[i]);
    }
    monitor->baseline_timestamp = get_time_us();

    /* Allocate observation buffers */
    size_t obs_capacity = monitor->config.thresholds.observation_window_size;
    if (obs_capacity == 0) obs_capacity = 1000;

    monitor->observations.actions = nimcp_calloc(obs_capacity, sizeof(alignment_action_observation_t));
    monitor->observations.explanations = nimcp_calloc(obs_capacity, sizeof(alignment_explanation_observation_t));
    if (monitor->observations.actions == NULL || monitor->observations.explanations == NULL) {
        NIMCP_LOG_ERROR(LOG_CATEGORY, "Failed to allocate observation buffers");
        nimcp_free(monitor->observations.actions);
        nimcp_free(monitor->observations.explanations);
        nimcp_mutex_destroy(monitor->mutex);
        nimcp_free(monitor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "alignment_monitor_create: validation failed");
        return NULL;
    }
    monitor->observations.action_capacity = obs_capacity;
    monitor->observations.explanation_capacity = obs_capacity;

    /* Initialize timing */
    monitor->start_time = get_time_us();

    /* Set magic */
    monitor->magic = ALIGNMENT_MONITOR_MAGIC;

    NIMCP_LOG_INFO(LOG_CATEGORY,
        "Alignment monitor created with %u dimensions",
        monitor->config.active_dimensions);

    return monitor;
}

void alignment_monitor_destroy(alignment_monitor_t* monitor)
{
    if (!is_valid_handle(monitor)) {
        return;
    }

    /* Disconnect from bio-async */
    if (monitor->bio_async_connected) {
        bio_router_unregister_module(monitor->bio_ctx);
    }

    /* Invalidate magic */
    monitor->magic = 0;

    /* Free observation buffers */
    nimcp_free(monitor->observations.actions);
    nimcp_free(monitor->observations.explanations);

    /* Destroy mutex */
    if (monitor->mutex != NULL) {
        nimcp_mutex_destroy(monitor->mutex);
    }

    nimcp_free(monitor);

    NIMCP_LOG_INFO(LOG_CATEGORY, "Alignment monitor destroyed");
}

nimcp_error_t alignment_monitor_reset(alignment_monitor_t* monitor)
{
    if (!is_valid_handle(monitor)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "alignment_monitor: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    nimcp_mutex_lock(monitor->mutex);

    /* Reset current values to baseline */
    for (uint32_t i = 0; i < ALIGNMENT_VALUE_DIMENSIONS; i++) {
        monitor->current_values[i] = monitor->baseline_values[i];
        init_value_stats(&monitor->value_stats[i]);
    }

    /* Clear observation history */
    monitor->observations.action_count = 0;
    monitor->observations.action_index = 0;
    monitor->observations.explanation_count = 0;
    monitor->observations.explanation_index = 0;

    /* Clear drift events */
    monitor->drift_event_count = 0;
    monitor->drift_event_index = 0;

    /* Reset statistics */
    memset(&monitor->stats, 0, sizeof(monitor->stats));
    monitor->start_time = get_time_us();

    /* Invalidate cache */
    monitor->cached_status_valid = false;

    nimcp_mutex_unlock(monitor->mutex);

    NIMCP_LOG_INFO(LOG_CATEGORY, "Alignment monitor reset");

    return NIMCP_OK;
}

/* ============================================================================
 * Observation API
 * ============================================================================ */

nimcp_error_t alignment_monitor_observe_action(
    alignment_monitor_t* monitor,
    const alignment_action_observation_t* observation)
{
    if (!is_valid_handle(monitor) || observation == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "alignment_monitor: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    nimcp_mutex_lock(monitor->mutex);

    /* Store observation in circular buffer */
    size_t idx = monitor->observations.action_index;
    memcpy(&monitor->observations.actions[idx], observation, sizeof(*observation));
    monitor->observations.action_index = (idx + 1) % monitor->observations.action_capacity;
    if (monitor->observations.action_count < monitor->observations.action_capacity) {
        monitor->observations.action_count++;
    }

    /* Update value estimates from action */
    float alpha = monitor->config.smoothing_alpha;
    for (uint32_t i = 0; i < monitor->config.active_dimensions; i++) {
        float relevance = observation->value_relevance[i];
        if (fabsf(relevance) > 0.01f) {
            /* Update current value estimate */
            float value_signal = observation->was_positive ?
                                 observation->intensity : -observation->intensity;
            value_signal *= relevance;

            if (monitor->config.enable_exponential_smoothing) {
                monitor->current_values[i] = (1.0f - alpha) * monitor->current_values[i] +
                                             alpha * (monitor->baseline_values[i] + value_signal);
            } else {
                monitor->current_values[i] += value_signal * 0.01f;
            }

            /* Clamp values to [0, 1] range */
            if (monitor->current_values[i] < 0.0f) monitor->current_values[i] = 0.0f;
            if (monitor->current_values[i] > 1.0f) monitor->current_values[i] = 1.0f;

            /* Update running stats */
            update_value_stats(&monitor->value_stats[i], monitor->current_values[i]);
        }
    }

    /* Update statistics */
    monitor->stats.total_observations++;
    monitor->stats.action_observations++;

    /* Invalidate cache */
    monitor->cached_status_valid = false;

    nimcp_mutex_unlock(monitor->mutex);

    return NIMCP_OK;
}

nimcp_error_t alignment_monitor_observe_explanation(
    alignment_monitor_t* monitor,
    const alignment_explanation_observation_t* observation)
{
    if (!is_valid_handle(monitor) || observation == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "alignment_monitor: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    nimcp_mutex_lock(monitor->mutex);

    /* Store observation */
    size_t idx = monitor->observations.explanation_index;
    memcpy(&monitor->observations.explanations[idx], observation, sizeof(*observation));
    monitor->observations.explanation_index = (idx + 1) % monitor->observations.explanation_capacity;
    if (monitor->observations.explanation_count < monitor->observations.explanation_capacity) {
        monitor->observations.explanation_count++;
    }

    /* Update statistics */
    monitor->stats.total_observations++;
    monitor->stats.explanation_observations++;

    /* Invalidate cache */
    monitor->cached_status_valid = false;

    nimcp_mutex_unlock(monitor->mutex);

    return NIMCP_OK;
}

nimcp_error_t alignment_monitor_observe_values(
    alignment_monitor_t* monitor,
    const float* values,
    float confidence)
{
    if (!is_valid_handle(monitor) || values == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "alignment_monitor: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    nimcp_mutex_lock(monitor->mutex);

    /* Blend observed values with current estimate based on confidence */
    for (uint32_t i = 0; i < monitor->config.active_dimensions; i++) {
        monitor->current_values[i] = (1.0f - confidence) * monitor->current_values[i] +
                                     confidence * values[i];
        /* Clamp values to [0, 1] range */
        if (monitor->current_values[i] < 0.0f) monitor->current_values[i] = 0.0f;
        if (monitor->current_values[i] > 1.0f) monitor->current_values[i] = 1.0f;
        update_value_stats(&monitor->value_stats[i], monitor->current_values[i]);
    }

    monitor->stats.total_observations++;
    monitor->cached_status_valid = false;

    nimcp_mutex_unlock(monitor->mutex);

    return NIMCP_OK;
}

/* ============================================================================
 * Drift Detection API
 * ============================================================================ */

nimcp_error_t alignment_monitor_check_drift(
    alignment_monitor_t* monitor,
    const alignment_thresholds_t* thresholds,
    bool* drift_detected,
    alignment_status_t* status)
{
    if (!is_valid_handle(monitor) || drift_detected == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "alignment_monitor: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    nimcp_mutex_lock(monitor->mutex);

    const alignment_thresholds_t* thresh = thresholds ? thresholds : &monitor->config.thresholds;
    alignment_status_t local_status;
    memset(&local_status, 0, sizeof(local_status));

    /* Copy baseline */
    memcpy(local_status.baseline_values, monitor->baseline_values, sizeof(local_status.baseline_values));
    memcpy(local_status.baseline_hash, monitor->baseline_hash, sizeof(local_status.baseline_hash));
    local_status.baseline_timestamp = monitor->baseline_timestamp;

    /* Copy current values */
    memcpy(local_status.current_values, monitor->current_values, sizeof(local_status.current_values));
    memcpy(local_status.value_stats, monitor->value_stats, sizeof(local_status.value_stats));

    /* Compute cosine similarity */
    local_status.cosine_similarity_to_baseline = compute_cosine_similarity(
        monitor->current_values, monitor->baseline_values, monitor->config.active_dimensions);

    /* Compute Euclidean distance */
    local_status.euclidean_distance = compute_euclidean_distance(
        monitor->current_values, monitor->baseline_values, monitor->config.active_dimensions);

    /* Find max dimension drift */
    local_status.max_dimension_drift = 0.0f;
    for (uint32_t i = 0; i < monitor->config.active_dimensions; i++) {
        float drift = fabsf(monitor->current_values[i] - monitor->baseline_values[i]);
        if (drift > local_status.max_dimension_drift) {
            local_status.max_dimension_drift = drift;
            local_status.most_drifted_dimension = i;
        }
    }

    /* Compute KL and JS divergence */
    float baseline_probs[ALIGNMENT_VALUE_DIMENSIONS];
    float current_probs[ALIGNMENT_VALUE_DIMENSIONS];
    normalize_to_distribution(monitor->baseline_values, baseline_probs, monitor->config.active_dimensions);
    normalize_to_distribution(monitor->current_values, current_probs, monitor->config.active_dimensions);

    local_status.kl_divergence = nimcp_stats_kl_divergence(
        current_probs, baseline_probs, monitor->config.active_dimensions);
    local_status.js_divergence = nimcp_stats_js_divergence(
        current_probs, baseline_probs, monitor->config.active_dimensions);

    /* Compute consistency scores */
    local_status.action_consistency_score = 1.0f;
    local_status.explanation_consistency_score = 1.0f;

    if (monitor->observations.explanation_count > 0) {
        /* Compare stated values in explanations to inferred values */
        float sum_diff = 0.0f;
        for (size_t i = 0; i < monitor->observations.explanation_count; i++) {
            const alignment_explanation_observation_t* exp = &monitor->observations.explanations[i];
            for (uint32_t j = 0; j < monitor->config.active_dimensions; j++) {
                sum_diff += fabsf(exp->stated_values[j] - monitor->current_values[j]);
            }
        }
        float avg_diff = sum_diff / (monitor->observations.explanation_count * monitor->config.active_dimensions);
        local_status.explanation_consistency_score = 1.0f - fminf(avg_diff, 1.0f);
    }

    /* Check for drift */
    bool drift = false;

    if (local_status.cosine_similarity_to_baseline < thresh->cosine_similarity_min) {
        drift = true;
        NIMCP_LOG_WARN(LOG_CATEGORY, "Drift: cosine_similarity=%.3f < %.3f",
            local_status.cosine_similarity_to_baseline, thresh->cosine_similarity_min);
    }

    if (local_status.kl_divergence > thresh->kl_divergence_threshold) {
        drift = true;
        NIMCP_LOG_WARN(LOG_CATEGORY, "Drift: kl_divergence=%.3f > %.3f",
            local_status.kl_divergence, thresh->kl_divergence_threshold);
    }

    if (local_status.js_divergence > thresh->js_divergence_threshold) {
        drift = true;
        NIMCP_LOG_WARN(LOG_CATEGORY, "Drift: js_divergence=%.3f > %.3f",
            local_status.js_divergence, thresh->js_divergence_threshold);
    }

    if (local_status.max_dimension_drift > thresh->max_single_dimension_drift) {
        drift = true;
        NIMCP_LOG_WARN(LOG_CATEGORY, "Drift: max_dimension_drift=%.3f > %.3f (dim=%u)",
            local_status.max_dimension_drift, thresh->max_single_dimension_drift,
            local_status.most_drifted_dimension);
    }

    local_status.drift_detected = drift;

    /* Compute overall alignment score */
    local_status.overall_alignment_score =
        0.4f * local_status.cosine_similarity_to_baseline +
        0.3f * (1.0f - fminf(local_status.js_divergence, 1.0f)) +
        0.15f * local_status.action_consistency_score +
        0.15f * local_status.explanation_consistency_score;

    /* Record drift event if detected */
    if (drift) {
        alignment_drift_event_t event;
        memset(&event, 0, sizeof(event));
        event.timestamp = get_time_us();
        event.dimension = local_status.most_drifted_dimension;
        event.old_value = monitor->baseline_values[event.dimension];
        event.new_value = monitor->current_values[event.dimension];
        event.drift_amount = local_status.max_dimension_drift;
        event.kl_divergence = local_status.kl_divergence;
        event.js_divergence = local_status.js_divergence;
        safe_strcpy(event.reason, "Threshold exceeded", sizeof(event.reason));
        event.escalated_to_tripwire = monitor->config.connect_to_tripwires;

        add_drift_event(monitor, &event);
        monitor->stats.drift_events_detected++;
        if (event.escalated_to_tripwire) {
            monitor->stats.tripwire_escalations++;
        }
    }

    /* Update statistics */
    monitor->stats.current_alignment_score = local_status.overall_alignment_score;
    if (local_status.overall_alignment_score < monitor->stats.min_alignment_score_observed ||
        monitor->stats.min_alignment_score_observed == 0.0f) {
        monitor->stats.min_alignment_score_observed = local_status.overall_alignment_score;
    }

    /* Cache result */
    memcpy(&monitor->cached_status, &local_status, sizeof(local_status));
    monitor->cached_status_time = get_time_us();
    monitor->cached_status_valid = true;

    /* Output */
    *drift_detected = drift;
    if (status != NULL) {
        memcpy(status, &local_status, sizeof(*status));
    }

    nimcp_mutex_unlock(monitor->mutex);

    return NIMCP_OK;
}

nimcp_error_t alignment_monitor_infer_values(
    alignment_monitor_t* monitor,
    float* inferred_values)
{
    if (!is_valid_handle(monitor) || inferred_values == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "alignment_monitor: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    nimcp_mutex_lock(monitor->mutex);

    memcpy(inferred_values, monitor->current_values,
           ALIGNMENT_VALUE_DIMENSIONS * sizeof(float));

    nimcp_mutex_unlock(monitor->mutex);
    return NIMCP_OK;
}

nimcp_error_t alignment_monitor_get_status(
    const alignment_monitor_t* monitor,
    alignment_status_t* status)
{
    if (!is_valid_handle(monitor) || status == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "alignment_monitor: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    alignment_monitor_t* mutable_monitor = (alignment_monitor_t*)monitor;
    nimcp_mutex_lock(mutable_monitor->mutex);

    if (monitor->cached_status_valid) {
        memcpy(status, &monitor->cached_status, sizeof(*status));
    } else {
        /* Compute fresh status */
        bool drift;
        nimcp_mutex_unlock(mutable_monitor->mutex);
        return alignment_monitor_check_drift(mutable_monitor, NULL, &drift, status);
    }

    nimcp_mutex_unlock(mutable_monitor->mutex);
    return NIMCP_OK;
}

/* ============================================================================
 * Divergence Metrics API
 * ============================================================================ */

nimcp_error_t alignment_monitor_compute_kl_divergence(
    const alignment_monitor_t* monitor,
    float* kl_divergence)
{
    if (!is_valid_handle(monitor) || kl_divergence == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "alignment_monitor: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    alignment_monitor_t* mutable_monitor = (alignment_monitor_t*)monitor;
    nimcp_mutex_lock(mutable_monitor->mutex);

    float baseline_probs[ALIGNMENT_VALUE_DIMENSIONS];
    float current_probs[ALIGNMENT_VALUE_DIMENSIONS];
    normalize_to_distribution(monitor->baseline_values, baseline_probs, monitor->config.active_dimensions);
    normalize_to_distribution(monitor->current_values, current_probs, monitor->config.active_dimensions);

    *kl_divergence = nimcp_stats_kl_divergence(
        current_probs, baseline_probs, monitor->config.active_dimensions);

    nimcp_mutex_unlock(mutable_monitor->mutex);
    return NIMCP_OK;
}

nimcp_error_t alignment_monitor_compute_js_divergence(
    const alignment_monitor_t* monitor,
    float* js_divergence)
{
    if (!is_valid_handle(monitor) || js_divergence == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "alignment_monitor: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    alignment_monitor_t* mutable_monitor = (alignment_monitor_t*)monitor;
    nimcp_mutex_lock(mutable_monitor->mutex);

    float baseline_probs[ALIGNMENT_VALUE_DIMENSIONS];
    float current_probs[ALIGNMENT_VALUE_DIMENSIONS];
    normalize_to_distribution(monitor->baseline_values, baseline_probs, monitor->config.active_dimensions);
    normalize_to_distribution(monitor->current_values, current_probs, monitor->config.active_dimensions);

    *js_divergence = nimcp_stats_js_divergence(
        current_probs, baseline_probs, monitor->config.active_dimensions);

    nimcp_mutex_unlock(mutable_monitor->mutex);
    return NIMCP_OK;
}

nimcp_error_t alignment_monitor_compute_mutual_info(
    const alignment_monitor_t* monitor,
    float* mutual_info)
{
    if (!is_valid_handle(monitor) || mutual_info == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "alignment_monitor: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    /* For now, return a placeholder */
    *mutual_info = 0.5f;
    return NIMCP_OK;
}

nimcp_error_t alignment_monitor_compute_cosine_similarity(
    const alignment_monitor_t* monitor,
    float* similarity)
{
    if (!is_valid_handle(monitor) || similarity == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "alignment_monitor: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    alignment_monitor_t* mutable_monitor = (alignment_monitor_t*)monitor;
    nimcp_mutex_lock(mutable_monitor->mutex);

    *similarity = compute_cosine_similarity(
        monitor->current_values, monitor->baseline_values, monitor->config.active_dimensions);

    nimcp_mutex_unlock(mutable_monitor->mutex);
    return NIMCP_OK;
}

/* ============================================================================
 * Status API
 * ============================================================================ */

nimcp_error_t alignment_monitor_get_stats(
    const alignment_monitor_t* monitor,
    alignment_monitor_stats_t* stats)
{
    if (!is_valid_handle(monitor) || stats == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "alignment_monitor: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    alignment_monitor_t* mutable_monitor = (alignment_monitor_t*)monitor;
    nimcp_mutex_lock(mutable_monitor->mutex);

    memcpy(stats, &monitor->stats, sizeof(*stats));
    stats->monitoring_uptime_ms = (get_time_us() - monitor->start_time) / 1000;

    nimcp_mutex_unlock(mutable_monitor->mutex);
    return NIMCP_OK;
}

nimcp_error_t alignment_monitor_get_drift_events(
    const alignment_monitor_t* monitor,
    alignment_drift_event_t* events,
    size_t max_events,
    size_t* count_out)
{
    if (!is_valid_handle(monitor) || events == NULL || count_out == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "alignment_monitor: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    alignment_monitor_t* mutable_monitor = (alignment_monitor_t*)monitor;
    nimcp_mutex_lock(mutable_monitor->mutex);

    size_t count = monitor->drift_event_count;
    if (count > max_events) {
        count = max_events;
    }

    for (size_t i = 0; i < count; i++) {
        size_t idx = (monitor->drift_event_index + ALIGNMENT_MAX_DRIFT_EVENTS - count + i)
                     % ALIGNMENT_MAX_DRIFT_EVENTS;
        memcpy(&events[i], &monitor->drift_events[idx], sizeof(events[i]));
    }

    *count_out = count;

    nimcp_mutex_unlock(mutable_monitor->mutex);
    return NIMCP_OK;
}

nimcp_error_t alignment_monitor_get_baseline(
    const alignment_monitor_t* monitor,
    float* baseline)
{
    if (!is_valid_handle(monitor) || baseline == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "alignment_monitor: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    alignment_monitor_t* mutable_monitor = (alignment_monitor_t*)monitor;
    nimcp_mutex_lock(mutable_monitor->mutex);

    memcpy(baseline, monitor->baseline_values, ALIGNMENT_VALUE_DIMENSIONS * sizeof(float));

    nimcp_mutex_unlock(mutable_monitor->mutex);
    return NIMCP_OK;
}

nimcp_error_t alignment_monitor_get_dimension(
    const alignment_monitor_t* monitor,
    uint32_t dimension,
    value_dimension_t* info)
{
    if (!is_valid_handle(monitor) || info == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "alignment_monitor: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    if (dimension >= ALIGNMENT_VALUE_DIMENSIONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "alignment_monitor: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    alignment_monitor_t* mutable_monitor = (alignment_monitor_t*)monitor;
    nimcp_mutex_lock(mutable_monitor->mutex);

    memcpy(info, &monitor->config.value_dimensions[dimension], sizeof(*info));
    info->current_value = monitor->current_values[dimension];

    nimcp_mutex_unlock(mutable_monitor->mutex);
    return NIMCP_OK;
}

/* ============================================================================
 * Integration API
 * ============================================================================ */

nimcp_error_t alignment_monitor_connect_bio_async(alignment_monitor_t* monitor)
{
    if (!is_valid_handle(monitor)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "alignment_monitor: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    nimcp_mutex_lock(monitor->mutex);

    if (monitor->bio_async_connected) {
        nimcp_mutex_unlock(monitor->mutex);
        return NIMCP_OK;
    }

    bio_module_info_t module_info = {
        .module_id = BIO_MODULE_ALIGNMENT_MONITOR,
        .module_name = "alignment_monitor",
        .inbox_capacity = 0,
        .user_data = monitor
    };
    monitor->bio_ctx = bio_router_register_module(&module_info);
    if (!monitor->bio_ctx) {
        NIMCP_LOG_WARN(LOG_CATEGORY, "Failed to connect to bio-async");
        nimcp_mutex_unlock(monitor->mutex);
        return NIMCP_OK;  /* Non-fatal */
    }

    monitor->bio_async_connected = true;
    nimcp_mutex_unlock(monitor->mutex);

    NIMCP_LOG_INFO(LOG_CATEGORY, "Connected to bio-async messaging");
    return NIMCP_OK;
}

nimcp_error_t alignment_monitor_connect_tripwires(
    alignment_monitor_t* monitor,
    void* tripwires)
{
    if (!is_valid_handle(monitor)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "alignment_monitor: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    nimcp_mutex_lock(monitor->mutex);
    monitor->tripwires = tripwires;
    nimcp_mutex_unlock(monitor->mutex);

    NIMCP_LOG_INFO(LOG_CATEGORY, "Connected to tripwire system");
    return NIMCP_OK;
}

nimcp_error_t alignment_monitor_connect_value_commitment(
    alignment_monitor_t* monitor,
    void* commitment)
{
    if (!is_valid_handle(monitor)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "alignment_monitor: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    nimcp_mutex_lock(monitor->mutex);
    monitor->value_commitment = commitment;
    nimcp_mutex_unlock(monitor->mutex);

    NIMCP_LOG_INFO(LOG_CATEGORY, "Connected to value commitment system");
    return NIMCP_OK;
}

nimcp_error_t alignment_monitor_connect_brain_immune(
    alignment_monitor_t* monitor,
    struct brain_immune* brain_immune)
{
    if (!is_valid_handle(monitor)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "alignment_monitor: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    nimcp_mutex_lock(monitor->mutex);
    monitor->brain_immune = brain_immune;
    nimcp_mutex_unlock(monitor->mutex);

    NIMCP_LOG_INFO(LOG_CATEGORY, "Connected to brain immune system");
    return NIMCP_OK;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

void alignment_monitor_default_dimension_names(
    const char** names,
    uint32_t* count)
{
    if (names != NULL) {
        *names = DEFAULT_VALUE_NAMES[0];
    }
    if (count != NULL) {
        *count = DEFAULT_VALUE_COUNT;
    }
}

size_t alignment_monitor_format_status(
    const alignment_status_t* status,
    char* buffer,
    size_t buffer_size)
{
    if (status == NULL || buffer == NULL || buffer_size == 0) {
        return 0;
    }

    int written = snprintf(buffer, buffer_size,
        "Alignment Status:\n"
        "  Overall Score: %.3f\n"
        "  Cosine Similarity: %.3f\n"
        "  KL Divergence: %.3f\n"
        "  JS Divergence: %.3f\n"
        "  Max Dimension Drift: %.3f (dim %u)\n"
        "  Drift Detected: %s\n",
        status->overall_alignment_score,
        status->cosine_similarity_to_baseline,
        status->kl_divergence,
        status->js_divergence,
        status->max_dimension_drift,
        status->most_drifted_dimension,
        status->drift_detected ? "YES" : "NO");

    return (size_t)written;
}
