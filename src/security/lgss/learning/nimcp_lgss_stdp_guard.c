/**
 * @file nimcp_lgss_stdp_guard.c
 * @brief Implementation of LGSS STDP Guard
 * @version 1.0.0
 * @date 2026-01-16
 *
 * Implements the STDP guard that validates spike-timing-dependent plasticity
 * updates to prevent exploitation of spike timing patterns.
 */

#include "security/lgss/learning/nimcp_lgss_stdp_guard.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(lgss_stdp_guard, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

/** Spike timing record for rate tracking */
typedef struct {
    uint64_t* timestamps;           /**< Circular buffer of spike times */
    uint32_t capacity;              /**< Buffer capacity */
    uint32_t head;                  /**< Write position */
    uint32_t count;                 /**< Current count */
} spike_buffer_t;

/** Per-synapse tracking data */
typedef struct synapse_tracker {
    uint64_t synapse_id;
    spike_buffer_t pre_spikes;      /**< Pre-synaptic spike times */
    spike_buffer_t post_spikes;     /**< Post-synaptic spike times */
    float cumulative_change;        /**< Cumulative STDP change */
    uint64_t last_update_time;      /**< Last update timestamp */
    bool occupied;                  /**< Is this entry in use */
} synapse_tracker_t;

/** Internal STDP guard structure */
struct stdp_guard_internal {
    uint32_t magic;                     /**< Magic number for validation */

    /* Configuration */
    stdp_guard_config_t config;

    /* Base plasticity guard */
    plasticity_guard_t base_guard;

    /* Per-synapse tracking (hashmap) */
    synapse_tracker_t* trackers;        /**< Hashmap of synapse trackers */
    uint32_t tracker_hashmap_size;      /**< Hashmap size */
    uint32_t active_trackers;           /**< Number of active trackers */

    /* Global tracking for patterns */
    float* dt_history;                  /**< Recent dt values for pattern detection */
    uint32_t dt_history_size;           /**< History buffer size */
    uint32_t dt_history_head;           /**< Write position */
    uint32_t dt_history_count;          /**< Current count */

    /* Statistics */
    stdp_guard_stats_t stats;
};

/* ============================================================================
 * INTERNAL HELPERS
 * ============================================================================ */

static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static uint32_t hash_synapse_id(uint64_t id, uint32_t size) {
    uint64_t hash = id * 0x9E3779B97F4A7C15ULL;
    return (uint32_t)(hash % size);
}

static bool spike_buffer_init(spike_buffer_t* buf, uint32_t capacity) {
    buf->timestamps = nimcp_calloc(capacity, sizeof(uint64_t));
    if (!buf->timestamps) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "spike_buffer_init: buf->timestamps is NULL");
        return false;
    }
    buf->capacity = capacity;
    buf->head = 0;
    buf->count = 0;
    return true;
}

static void spike_buffer_destroy(spike_buffer_t* buf) {
    if (buf->timestamps) {
        nimcp_free(buf->timestamps);
        buf->timestamps = NULL;
    }
}

static void spike_buffer_add(spike_buffer_t* buf, uint64_t timestamp) {
    buf->timestamps[buf->head] = timestamp;
    buf->head = (buf->head + 1) % buf->capacity;
    if (buf->count < buf->capacity) {
        buf->count++;
    }
}

static uint32_t spike_buffer_count_within(spike_buffer_t* buf,
                                          uint64_t current_time,
                                          uint64_t window_us) {
    uint32_t count = 0;
    uint64_t cutoff = current_time > window_us ? current_time - window_us : 0;

    for (uint32_t i = 0; i < buf->count; i++) {
        if (buf->timestamps[i] >= cutoff) {
            count++;
        }
    }
    return count;
}

static synapse_tracker_t* get_or_create_tracker(struct stdp_guard_internal* g,
                                                 uint64_t synapse_id) {
    uint32_t idx = hash_synapse_id(synapse_id, g->tracker_hashmap_size);
    uint32_t start = idx;

    /* First pass: look for existing or empty slot */
    do {
        if (!g->trackers[idx].occupied) {
            /* Create new tracker */
            synapse_tracker_t* t = &g->trackers[idx];
            t->synapse_id = synapse_id;
            t->occupied = true;

            if (!spike_buffer_init(&t->pre_spikes, 64)) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "spike_buffer_add: spike_buffer_init is NULL");
                return NULL;
            }
            if (!spike_buffer_init(&t->post_spikes, 64)) {
                spike_buffer_destroy(&t->pre_spikes);
                t->occupied = false;
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "spike_buffer_add: spike_buffer_init is NULL");
                return NULL;
            }

            t->cumulative_change = 0.0f;
            t->last_update_time = get_time_us();
            g->active_trackers++;
            return t;
        }
        if (g->trackers[idx].synapse_id == synapse_id) {
            return &g->trackers[idx];
        }
        idx = (idx + 1) % g->tracker_hashmap_size;
    } while (idx != start);

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spike_buffer_add: validation failed");
    return NULL; /* Hashmap full */
}

/**
 * Compute standard STDP delta using exponential windows
 */
static float compute_stdp_delta(const stdp_guard_config_t* config, float dt_ms) {
    if (dt_ms > config->stdp_window_ms || dt_ms < -config->stdp_window_ms) {
        return 0.0f; /* Outside STDP window */
    }

    if (dt_ms > 0) {
        /* Pre before Post -> LTP */
        return config->max_ltp_amplitude * expf(-dt_ms / config->tau_plus);
    } else {
        /* Post before Pre -> LTD */
        return -config->max_ltd_amplitude * expf(dt_ms / config->tau_minus);
    }
}

/**
 * Detect suspicious regularity in inter-spike intervals
 */
static bool detect_timing_regularity(struct stdp_guard_internal* g) {
    if (g->dt_history_count < 10) {
        return false;
    }

    /* Calculate variance of dt values */
    float sum = 0.0f, sum_sq = 0.0f;
    for (uint32_t i = 0; i < g->dt_history_count; i++) {
        sum += g->dt_history[i];
        sum_sq += g->dt_history[i] * g->dt_history[i];
    }

    float mean = sum / g->dt_history_count;
    float variance = (sum_sq / g->dt_history_count) - (mean * mean);
    float stddev = sqrtf(fabsf(variance));

    /* Very low variance indicates suspiciously regular timing */
    float cv = (mean != 0) ? stddev / fabsf(mean) : 1.0f;
    return cv < g->config.timing_regularity_threshold;
}

/**
 * Detect burst activity
 */
static bool detect_burst(spike_buffer_t* buf, uint32_t threshold_count,
                         float interval_ms, uint32_t* burst_count_out) {
    if (buf->count < threshold_count) {
        return false;
    }

    uint64_t current_time = get_time_us();
    uint64_t interval_us = (uint64_t)(interval_ms * 1000.0f);
    uint32_t count = spike_buffer_count_within(buf, current_time, interval_us);

    if (burst_count_out) *burst_count_out = count;
    return count >= threshold_count;
}

/* ============================================================================
 * PUBLIC API IMPLEMENTATION
 * ============================================================================ */

stdp_guard_config_t stdp_guard_default_config(void) {
    stdp_guard_config_t config = {
        .stdp_window_ms = STDP_DEFAULT_WINDOW_MS,
        .tau_plus = STDP_DEFAULT_TAU_PLUS,
        .tau_minus = STDP_DEFAULT_TAU_MINUS,
        .max_ltp_amplitude = STDP_DEFAULT_MAX_LTP,
        .max_ltd_amplitude = STDP_DEFAULT_MAX_LTD,
        .asymmetry_ratio_min = 0.5f,
        .asymmetry_ratio_max = 2.0f,
        .max_cumulative_change = 0.1f,
        .cumulative_window_ms = 1000.0f,
        .max_pre_spike_rate = STDP_DEFAULT_MAX_SPIKE_RATE,
        .max_post_spike_rate = STDP_DEFAULT_MAX_SPIKE_RATE,
        .spike_rate_window_sec = STDP_DEFAULT_RATE_WINDOW_SEC,
        .enable_burst_detection = true,
        .burst_threshold_count = 10,
        .burst_threshold_interval_ms = 50.0f,
        .enable_timing_pattern_detection = true,
        .timing_regularity_threshold = 0.1f,
        .use_base_guard = true,
        .clamp_to_limits = true,
        .enable_violation_logging = true,
        .enable_statistics = true
    };
    return config;
}

stdp_guard_t stdp_guard_create(
    const stdp_guard_config_t* config,
    plasticity_guard_t base_guard
) {
    struct stdp_guard_internal* guard = nimcp_calloc(1, sizeof(*guard));
    if (!guard) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "guard is NULL");

        return NULL;

    }

    guard->magic = LGSS_STDP_GUARD_MAGIC;

    if (config) {
        guard->config = *config;
    } else {
        guard->config = stdp_guard_default_config();
    }

    guard->base_guard = base_guard;

    /* Initialize synapse tracker hashmap */
    guard->tracker_hashmap_size = 4096;
    guard->trackers = nimcp_calloc(guard->tracker_hashmap_size, sizeof(synapse_tracker_t));
    if (!guard->trackers) {
        nimcp_free(guard);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stdp_guard_create: guard->trackers is NULL");
        return NULL;
    }

    /* Initialize dt history for pattern detection */
    guard->dt_history_size = 256;
    guard->dt_history = nimcp_calloc(guard->dt_history_size, sizeof(float));
    if (!guard->dt_history) {
        nimcp_free(guard->trackers);
        nimcp_free(guard);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stdp_guard_create: guard->dt_history is NULL");
        return NULL;
    }

    return guard;
}

void stdp_guard_destroy(stdp_guard_t guard) {
    if (!guard) return;

    struct stdp_guard_internal* g = guard;
    if (g->magic != LGSS_STDP_GUARD_MAGIC) return;

    /* Clean up trackers */
    for (uint32_t i = 0; i < g->tracker_hashmap_size; i++) {
        if (g->trackers[i].occupied) {
            spike_buffer_destroy(&g->trackers[i].pre_spikes);
            spike_buffer_destroy(&g->trackers[i].post_spikes);
        }
    }
    nimcp_free(g->trackers);
    nimcp_free(g->dt_history);

    g->magic = 0;
    nimcp_free(g);
}

int stdp_guard_reset(stdp_guard_t guard) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");

    struct stdp_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_STDP_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid STDP guard magic");

    /* Reset all trackers */
    for (uint32_t i = 0; i < g->tracker_hashmap_size; i++) {
        if (g->trackers[i].occupied) {
            g->trackers[i].pre_spikes.head = 0;
            g->trackers[i].pre_spikes.count = 0;
            g->trackers[i].post_spikes.head = 0;
            g->trackers[i].post_spikes.count = 0;
            g->trackers[i].cumulative_change = 0.0f;
        }
    }

    /* Reset dt history */
    g->dt_history_head = 0;
    g->dt_history_count = 0;

    /* Reset statistics */
    memset(&g->stats, 0, sizeof(g->stats));

    return NIMCP_SUCCESS;
}

int stdp_guard_process_spike_pair(
    stdp_guard_t guard,
    const stdp_spike_pair_t* pair,
    stdp_update_result_t* result
) {
    NIMCP_CHECK_THROW(guard && pair && result, NIMCP_ERROR_NULL_POINTER, "guard, pair, or result is NULL");

    struct stdp_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_STDP_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid STDP guard magic");

    uint64_t start_time = get_time_us();

    /* Initialize result */
    memset(result, 0, sizeof(*result));
    result->allowed = true;

    /* Update statistics */
    g->stats.total_pairs_processed++;

    /* Get or create tracker for this synapse */
    synapse_tracker_t* tracker = get_or_create_tracker(g, pair->synapse_id);
    if (!tracker) {
        result->allowed = false;
        snprintf(result->reason, sizeof(result->reason), "Tracker allocation failed");
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Compute raw STDP delta */
    float raw_delta = compute_stdp_delta(&g->config, pair->dt_ms);
    result->original_delta = raw_delta;
    result->adjusted_delta = raw_delta;

    /* Determine LTP vs LTD */
    if (raw_delta > 0) {
        result->ltp_component = raw_delta;
        g->stats.ltp_events++;
        g->stats.total_ltp_magnitude += raw_delta;
        if (raw_delta > g->stats.max_ltp_seen) {
            g->stats.max_ltp_seen = raw_delta;
        }
    } else if (raw_delta < 0) {
        result->ltd_component = -raw_delta;
        g->stats.ltd_events++;
        g->stats.total_ltd_magnitude += (-raw_delta);
        if (-raw_delta > g->stats.max_ltd_seen) {
            g->stats.max_ltd_seen = -raw_delta;
        }
    }

    /* Check if outside STDP window */
    if (pair->dt_ms > g->config.stdp_window_ms ||
        pair->dt_ms < -g->config.stdp_window_ms) {
        result->violations |= STDP_VIOLATION_WINDOW_EXCEEDED;
        g->stats.window_exceeded_violations++;
        /* Allow but zero the delta */
        result->adjusted_delta = 0.0f;
    }

    /* Check LTP amplitude limit */
    if (result->adjusted_delta > g->config.max_ltp_amplitude) {
        result->violations |= STDP_VIOLATION_LTP_LIMIT;
        g->stats.ltp_limit_violations++;
        if (g->config.clamp_to_limits) {
            result->adjusted_delta = g->config.max_ltp_amplitude;
            result->was_clamped = true;
        } else {
            result->allowed = false;
            snprintf(result->reason, sizeof(result->reason),
                    "LTP amplitude %.4f exceeds limit %.4f",
                    result->original_delta, g->config.max_ltp_amplitude);
        }
    }

    /* Check LTD amplitude limit */
    if (result->adjusted_delta < -g->config.max_ltd_amplitude) {
        result->violations |= STDP_VIOLATION_LTD_LIMIT;
        g->stats.ltd_limit_violations++;
        if (g->config.clamp_to_limits) {
            result->adjusted_delta = -g->config.max_ltd_amplitude;
            result->was_clamped = true;
        } else {
            result->allowed = false;
            snprintf(result->reason, sizeof(result->reason),
                    "LTD amplitude %.4f exceeds limit %.4f",
                    -result->original_delta, g->config.max_ltd_amplitude);
        }
    }

    /* Check spike rates */
    uint64_t current_time = get_time_us();
    uint64_t rate_window_us = (uint64_t)(g->config.spike_rate_window_sec * 1000000.0f);

    uint32_t pre_count = spike_buffer_count_within(&tracker->pre_spikes,
                                                    current_time, rate_window_us);
    float pre_rate = (float)pre_count / g->config.spike_rate_window_sec;

    uint32_t post_count = spike_buffer_count_within(&tracker->post_spikes,
                                                     current_time, rate_window_us);
    float post_rate = (float)post_count / g->config.spike_rate_window_sec;

    if (pre_rate > g->config.max_pre_spike_rate ||
        post_rate > g->config.max_post_spike_rate) {
        result->violations |= STDP_VIOLATION_SPIKE_RATE;
        g->stats.spike_rate_violations++;
        if (!g->config.clamp_to_limits) {
            result->allowed = false;
            snprintf(result->reason, sizeof(result->reason),
                    "Spike rate exceeded (pre: %.1f Hz, post: %.1f Hz)",
                    pre_rate, post_rate);
        }
    }

    /* Check cumulative change */
    float new_cumulative = tracker->cumulative_change + fabsf(result->adjusted_delta);
    if (new_cumulative > g->config.max_cumulative_change) {
        result->violations |= STDP_VIOLATION_CUMULATIVE;
        g->stats.cumulative_violations++;
        if (g->config.clamp_to_limits) {
            float allowed = g->config.max_cumulative_change - fabsf(tracker->cumulative_change);
            if (allowed < 0) allowed = 0;
            float sign = result->adjusted_delta >= 0 ? 1.0f : -1.0f;
            result->adjusted_delta = sign * allowed;
            result->was_clamped = true;
        } else {
            result->allowed = false;
            snprintf(result->reason, sizeof(result->reason),
                    "Cumulative change %.4f would exceed limit %.4f",
                    new_cumulative, g->config.max_cumulative_change);
        }
    }

    /* Burst detection */
    if (g->config.enable_burst_detection) {
        uint32_t burst_count;
        if (detect_burst(&tracker->pre_spikes, g->config.burst_threshold_count,
                        g->config.burst_threshold_interval_ms, &burst_count) ||
            detect_burst(&tracker->post_spikes, g->config.burst_threshold_count,
                        g->config.burst_threshold_interval_ms, &burst_count)) {
            result->violations |= STDP_VIOLATION_BURST_DETECTED;
            g->stats.burst_detections++;
            /* Allow but flag */
        }
    }

    /* Timing pattern detection */
    if (g->config.enable_timing_pattern_detection) {
        /* Add dt to history */
        g->dt_history[g->dt_history_head] = pair->dt_ms;
        g->dt_history_head = (g->dt_history_head + 1) % g->dt_history_size;
        if (g->dt_history_count < g->dt_history_size) {
            g->dt_history_count++;
        }

        if (detect_timing_regularity(g)) {
            result->violations |= STDP_VIOLATION_SUSPICIOUS_TIMING;
            g->stats.timing_pattern_violations++;
            /* Allow but flag */
        }
    }

    /* Check with base plasticity guard if configured */
    if (g->config.use_base_guard && g->base_guard) {
        float test_weight = pair->current_weight + result->adjusted_delta;
        if (plasticity_guard_would_violate(g->base_guard, pair->synapse_id,
                                           pair->current_weight, test_weight)) {
            result->violations |= STDP_VIOLATION_BASE_GUARD;
            result->allowed = false;
            snprintf(result->reason, sizeof(result->reason),
                    "Base plasticity guard rejected update");
        }
    }

    /* Update tracker state if allowed */
    if (result->allowed) {
        tracker->cumulative_change += fabsf(result->adjusted_delta);
        tracker->last_update_time = current_time;
        g->stats.pairs_allowed++;
        if (result->was_clamped) {
            g->stats.pairs_clamped++;
        }
    } else {
        g->stats.pairs_blocked++;
    }

    /* Update timing statistics */
    g->stats.avg_dt_ms = (g->stats.avg_dt_ms * (g->stats.total_pairs_processed - 1) +
                          pair->dt_ms) / g->stats.total_pairs_processed;

    /* Performance tracking */
    uint64_t elapsed = get_time_us() - start_time;
    g->stats.guard_overhead_us += elapsed;
    g->stats.avg_process_time_us = (float)g->stats.guard_overhead_us /
                                    (float)g->stats.total_pairs_processed;

    return NIMCP_SUCCESS;
}

uint32_t stdp_guard_process_batch(
    stdp_guard_t guard,
    const stdp_spike_pair_t* pairs,
    stdp_update_result_t* results,
    uint32_t count
) {
    if (!guard || !pairs || !results || count == 0) return 0;

    uint32_t allowed = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (stdp_guard_process_spike_pair(guard, &pairs[i], &results[i]) == NIMCP_SUCCESS) {
            if (results[i].allowed) {
                allowed++;
            }
        }
    }
    return allowed;
}

float stdp_guard_compute_raw_delta(stdp_guard_t guard, float dt_ms, float current_weight) {
    if (!guard) return 0.0f;

    struct stdp_guard_internal* g = guard;
    if (g->magic != LGSS_STDP_GUARD_MAGIC) return 0.0f;

    (void)current_weight; /* Could be used for weight-dependent STDP */
    return compute_stdp_delta(&g->config, dt_ms);
}

int stdp_guard_record_pre_spike(stdp_guard_t guard, uint64_t synapse_id, uint64_t spike_time_us) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");

    struct stdp_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_STDP_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid STDP guard magic");

    synapse_tracker_t* tracker = get_or_create_tracker(g, synapse_id);
    NIMCP_CHECK_THROW(tracker, NIMCP_ERROR_NO_MEMORY, "failed to create synapse tracker");

    spike_buffer_add(&tracker->pre_spikes, spike_time_us);
    return NIMCP_SUCCESS;
}

int stdp_guard_record_post_spike(stdp_guard_t guard, uint64_t synapse_id, uint64_t spike_time_us) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");

    struct stdp_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_STDP_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid STDP guard magic");

    synapse_tracker_t* tracker = get_or_create_tracker(g, synapse_id);
    NIMCP_CHECK_THROW(tracker, NIMCP_ERROR_NO_MEMORY, "failed to create synapse tracker");

    spike_buffer_add(&tracker->post_spikes, spike_time_us);
    return NIMCP_SUCCESS;
}

int stdp_guard_get_spike_rate(
    stdp_guard_t guard,
    uint64_t synapse_id,
    float* pre_rate_out,
    float* post_rate_out
) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");

    struct stdp_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_STDP_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid STDP guard magic");

    synapse_tracker_t* tracker = get_or_create_tracker(g, synapse_id);
    NIMCP_CHECK_THROW(tracker, NIMCP_ERROR_NOT_FOUND, "synapse tracker not found");

    uint64_t current_time = get_time_us();
    uint64_t window_us = (uint64_t)(g->config.spike_rate_window_sec * 1000000.0f);

    if (pre_rate_out) {
        uint32_t count = spike_buffer_count_within(&tracker->pre_spikes, current_time, window_us);
        *pre_rate_out = (float)count / g->config.spike_rate_window_sec;
    }
    if (post_rate_out) {
        uint32_t count = spike_buffer_count_within(&tracker->post_spikes, current_time, window_us);
        *post_rate_out = (float)count / g->config.spike_rate_window_sec;
    }

    return NIMCP_SUCCESS;
}

int stdp_guard_check_timing_regularity(stdp_guard_t guard, uint64_t synapse_id, bool* is_suspicious) {
    NIMCP_CHECK_THROW(guard && is_suspicious, NIMCP_ERROR_NULL_POINTER, "guard or is_suspicious is NULL");

    struct stdp_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_STDP_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid STDP guard magic");

    (void)synapse_id; /* Could implement per-synapse timing analysis */
    *is_suspicious = detect_timing_regularity(g);
    return NIMCP_SUCCESS;
}

int stdp_guard_detect_burst(
    stdp_guard_t guard,
    uint64_t synapse_id,
    bool* is_burst,
    uint32_t* burst_count
) {
    NIMCP_CHECK_THROW(guard && is_burst, NIMCP_ERROR_NULL_POINTER, "guard or is_burst is NULL");

    struct stdp_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_STDP_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid STDP guard magic");

    synapse_tracker_t* tracker = get_or_create_tracker(g, synapse_id);
    NIMCP_CHECK_THROW(tracker, NIMCP_ERROR_NOT_FOUND, "synapse tracker not found");

    *is_burst = detect_burst(&tracker->pre_spikes, g->config.burst_threshold_count,
                            g->config.burst_threshold_interval_ms, burst_count) ||
                detect_burst(&tracker->post_spikes, g->config.burst_threshold_count,
                            g->config.burst_threshold_interval_ms, burst_count);
    return NIMCP_SUCCESS;
}

int stdp_guard_get_cumulative_change(stdp_guard_t guard, uint64_t synapse_id, float* cumulative_out) {
    NIMCP_CHECK_THROW(guard && cumulative_out, NIMCP_ERROR_NULL_POINTER, "guard or cumulative_out is NULL");

    struct stdp_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_STDP_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid STDP guard magic");

    synapse_tracker_t* tracker = get_or_create_tracker(g, synapse_id);
    NIMCP_CHECK_THROW(tracker, NIMCP_ERROR_NOT_FOUND, "synapse tracker not found");

    *cumulative_out = tracker->cumulative_change;
    return NIMCP_SUCCESS;
}

int stdp_guard_reset_cumulative(stdp_guard_t guard, uint64_t synapse_id) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");

    struct stdp_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_STDP_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid STDP guard magic");

    synapse_tracker_t* tracker = get_or_create_tracker(g, synapse_id);
    NIMCP_CHECK_THROW(tracker, NIMCP_ERROR_NOT_FOUND, "synapse tracker not found");

    tracker->cumulative_change = 0.0f;
    return NIMCP_SUCCESS;
}

int stdp_guard_get_stats(stdp_guard_t guard, stdp_guard_stats_t* stats) {
    NIMCP_CHECK_THROW(guard && stats, NIMCP_ERROR_NULL_POINTER, "guard or stats is NULL");

    struct stdp_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_STDP_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid STDP guard magic");

    *stats = g->stats;

    /* Calculate derived statistics */
    if (g->stats.ltp_events > 0) {
        stats->avg_ltp_magnitude = g->stats.total_ltp_magnitude / g->stats.ltp_events;
    }
    if (g->stats.ltd_events > 0) {
        stats->avg_ltd_magnitude = g->stats.total_ltd_magnitude / g->stats.ltd_events;
    }

    return NIMCP_SUCCESS;
}

int stdp_guard_reset_stats(stdp_guard_t guard) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");

    struct stdp_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_STDP_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid STDP guard magic");

    memset(&g->stats, 0, sizeof(g->stats));
    return NIMCP_SUCCESS;
}

int stdp_guard_get_ltp_ltd_ratio(stdp_guard_t guard, float* ratio_out) {
    NIMCP_CHECK_THROW(guard && ratio_out, NIMCP_ERROR_NULL_POINTER, "guard or ratio_out is NULL");

    struct stdp_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_STDP_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid STDP guard magic");

    if (g->stats.total_ltd_magnitude > 0.0001f) {
        *ratio_out = g->stats.total_ltp_magnitude / g->stats.total_ltd_magnitude;
    } else if (g->stats.total_ltp_magnitude > 0.0001f) {
        *ratio_out = 1000.0f; /* Very high ratio if no LTD */
    } else {
        *ratio_out = 1.0f; /* Neutral if no activity */
    }

    return NIMCP_SUCCESS;
}

const char* stdp_violation_name(stdp_violation_t violation) {
    switch (violation) {
        case STDP_VIOLATION_NONE: return "NONE";
        case STDP_VIOLATION_LTP_LIMIT: return "LTP_LIMIT";
        case STDP_VIOLATION_LTD_LIMIT: return "LTD_LIMIT";
        case STDP_VIOLATION_SPIKE_RATE: return "SPIKE_RATE";
        case STDP_VIOLATION_WINDOW_EXCEEDED: return "WINDOW_EXCEEDED";
        case STDP_VIOLATION_ASYMMETRY: return "ASYMMETRY";
        case STDP_VIOLATION_CUMULATIVE: return "CUMULATIVE";
        case STDP_VIOLATION_SUSPICIOUS_TIMING: return "SUSPICIOUS_TIMING";
        case STDP_VIOLATION_BURST_DETECTED: return "BURST_DETECTED";
        case STDP_VIOLATION_BASE_GUARD: return "BASE_GUARD";
        default: return "UNKNOWN";
    }
}

int stdp_violations_to_string(stdp_violation_t violations, char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) return 0;

    buffer[0] = '\0';
    int written = 0;

    if (violations == STDP_VIOLATION_NONE) {
        return snprintf(buffer, buffer_size, "NONE");
    }

    const char* sep = "";
    for (int i = 0; i < 9; i++) {
        stdp_violation_t flag = (stdp_violation_t)(1 << i);
        if (violations & flag) {
            int n = snprintf(buffer + written, buffer_size - written,
                           "%s%s", sep, stdp_violation_name(flag));
            if (n > 0) {
                written += n;
                sep = "|";
            }
        }
    }

    return written;
}

void stdp_guard_print_summary(stdp_guard_t guard) {
    if (!guard) {
        printf("STDP Guard: NULL\n");
        return;
    }

    struct stdp_guard_internal* g = guard;
    if (g->magic != LGSS_STDP_GUARD_MAGIC) {
        printf("STDP Guard: INVALID (bad magic)\n");
        return;
    }

    printf("=== STDP Guard Summary ===\n");
    printf("Active trackers: %u\n", g->active_trackers);
    printf("Pairs processed: %lu\n", (unsigned long)g->stats.total_pairs_processed);
    printf("Allowed: %lu, Blocked: %lu, Clamped: %lu\n",
           (unsigned long)g->stats.pairs_allowed,
           (unsigned long)g->stats.pairs_blocked,
           (unsigned long)g->stats.pairs_clamped);
    printf("LTP events: %lu (avg: %.4f, max: %.4f)\n",
           (unsigned long)g->stats.ltp_events,
           g->stats.ltp_events > 0 ? g->stats.total_ltp_magnitude / g->stats.ltp_events : 0,
           g->stats.max_ltp_seen);
    printf("LTD events: %lu (avg: %.4f, max: %.4f)\n",
           (unsigned long)g->stats.ltd_events,
           g->stats.ltd_events > 0 ? g->stats.total_ltd_magnitude / g->stats.ltd_events : 0,
           g->stats.max_ltd_seen);

    float ratio;
    stdp_guard_get_ltp_ltd_ratio(guard, &ratio);
    printf("LTP/LTD ratio: %.3f\n", ratio);
    printf("Avg dt: %.2f ms\n", g->stats.avg_dt_ms);
    printf("==========================\n");
}
