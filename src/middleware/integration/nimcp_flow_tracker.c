#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_flow_tracker.c - Cross-Modal Information Flow Tracking
//=============================================================================
/**
 * @file nimcp_flow_tracker.c
 * @brief Cross-modal flow tracking for cognitive-middleware integration
 *
 * WHAT: Monitor information flow between middleware and cognitive layers
 * WHY:  Understand and optimize layer integration efficiency
 * HOW:  Tracks bits/sec, latency, efficiency η = I_out / I_in for 5 paths
 *
 * DESIGN:
 * - Per-path statistics: O(1) updates
 * - Latency histograms: Log-scale bins for p50/p90/p99
 * - NIMCP utils: nimcp_time, nimcp_memory, nimcp_logging
 * - Thread safety: Fine-grained per-path mutexes
 *
 * @author NIMCP Development Team
 * @date 2025-11-21
 */

#include "middleware/integration/nimcp_flow_tracker.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"

#define LOG_MODULE "middleware_flow_tracker"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(flow_tracker)

#include "utils/thread/nimcp_thread.h"
#include "security/nimcp_security.h"
#include "async/nimcp_bio_router.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

//=============================================================================
// Constants
//=============================================================================

#define LATENCY_HISTOGRAM_SIZE 32
#define MIN_LATENCY_US 1
#define MAX_LATENCY_US 1000000  // 1 second

//=============================================================================
// Internal Structures
//=============================================================================

typedef struct {
    uint32_t bins[LATENCY_HISTOGRAM_SIZE];
    uint32_t total_samples;
    float min_value_us;
    float max_value_us;
} latency_histogram_t;

typedef struct {
    // Flow rates
    float input_rate_bits_per_sec;
    float output_rate_bits_per_sec;
    float throughput_bits_per_sec;

    // Efficiency
    float flow_efficiency;
    float bottleneck_severity;

    // Capacity
    float channel_capacity_bits_per_sec;
    float capacity_utilization;

    // Latency histogram
    latency_histogram_t latency_hist;

    // Latency statistics
    float avg_latency_us;
    float min_latency_us;
    float max_latency_us;
    float p50_latency_us;
    float p90_latency_us;
    float p99_latency_us;
    float stddev_latency_us;

    // Counters
    uint64_t total_events;
    uint64_t filtered_events;
    uint64_t bottlenecked_events;

    // Information loss
    float information_loss_bits;
    float loss_percentage;

    // Timing
    uint64_t measurement_window_start_us;
    uint64_t last_update_time_us;

    // Accumulators
    float total_information_bits;
    float filtered_information_bits;
    uint64_t latency_sum_us;
    uint64_t latency_sq_sum_us;

    // Thread safety
    nimcp_mutex_t mutex;
} path_flow_state_t;

struct flow_tracker {
    // Configuration
    flow_tracker_config_t config;

    // Per-path state
    path_flow_state_t paths[FLOW_TRACKER_NUM_PATHS];

    // Global metrics
    float total_throughput_bits_per_sec;
    float avg_flow_efficiency;
    float worst_path_efficiency;
    integration_path_t worst_path;
    bool any_bottleneck_detected;
    uint32_t num_bottlenecked_paths;
    uint64_t last_global_update_us;

    // Bio-async integration
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;
};

//=============================================================================
// Helper Functions
//=============================================================================

static uint32_t latency_to_histogram_bin(float latency_us) {
    if (latency_us <= MIN_LATENCY_US) return 0;
    if (latency_us >= MAX_LATENCY_US) return LATENCY_HISTOGRAM_SIZE - 1;

    // Log-scale binning
    float log_val = log2f(latency_us / (float)MIN_LATENCY_US);
    float log_range = log2f((float)MAX_LATENCY_US / (float)MIN_LATENCY_US);
    uint32_t bin = (uint32_t)((log_val / log_range) * (LATENCY_HISTOGRAM_SIZE - 1));

    if (bin >= LATENCY_HISTOGRAM_SIZE) bin = LATENCY_HISTOGRAM_SIZE - 1;
    return bin;
}

static void update_latency_histogram(latency_histogram_t* hist, float latency_us) {
    uint32_t bin = latency_to_histogram_bin(latency_us);
    hist->bins[bin]++;
    hist->total_samples++;

    if (latency_us < hist->min_value_us || hist->total_samples == 1) {
        hist->min_value_us = latency_us;
    }
    if (latency_us > hist->max_value_us || hist->total_samples == 1) {
        hist->max_value_us = latency_us;
    }
}

static float calculate_percentile(const latency_histogram_t* hist, float percentile) {
    if (hist->total_samples == 0) return 0.0F;

    // For p99 with 100 samples, we want the value where 99% are below/equal
    // That means we want to exceed 99 samples, so target_count should be 99
    // and we check cumulative > target_count
    uint32_t target_count = (uint32_t)(hist->total_samples * percentile);
    uint32_t cumulative = 0;

    for (uint32_t i = 0; i < LATENCY_HISTOGRAM_SIZE; i++) {
        cumulative += hist->bins[i];
        if (cumulative > target_count) {
            // Interpolate within bin
            float bin_start = MIN_LATENCY_US * powf(
                (float)MAX_LATENCY_US / (float)MIN_LATENCY_US,
                (float)i / (float)(LATENCY_HISTOGRAM_SIZE - 1)
            );
            return bin_start;
        }
    }

    return hist->max_value_us;
}

static void update_path_metrics(path_flow_state_t* path) {
    uint64_t now_us = nimcp_time_get_us();
    uint64_t elapsed_us = now_us - path->measurement_window_start_us;

    // Calculate time-based metrics (rates, throughput)
    if (elapsed_us > 0) {
        float elapsed_sec = (float)elapsed_us / 1000000.0F;

        // Calculate rates
        path->throughput_bits_per_sec = path->total_information_bits / elapsed_sec;
    }

    // Calculate efficiency (independent of time)
    float total_input = path->total_information_bits + path->filtered_information_bits;
    if (total_input > 0.0F) {
        path->flow_efficiency = path->total_information_bits / total_input;
        path->loss_percentage = (path->filtered_information_bits / total_input) * 100.0F;
    }

    // Calculate latency statistics (independent of time)
    if (path->total_events > 0) {
        path->avg_latency_us = (float)path->latency_sum_us / (float)path->total_events;

        // Standard deviation
        float mean_sq = (float)path->latency_sq_sum_us / (float)path->total_events;
        float variance = mean_sq - (path->avg_latency_us * path->avg_latency_us);
        path->stddev_latency_us = sqrtf(fmaxf(0.0F, variance));

        // Percentiles from histogram
        path->p50_latency_us = calculate_percentile(&path->latency_hist, 0.50F);
        path->p90_latency_us = calculate_percentile(&path->latency_hist, 0.90F);
        path->p99_latency_us = calculate_percentile(&path->latency_hist, 0.99F);
    }

    path->last_update_time_us = now_us;
}

static const char* path_names[] = {
    "Middleware→Executive",
    "Middleware→Workspace",
    "Middleware→Introspection",
    "Executive→Middleware",
    "Workspace→Middleware"
};

//=============================================================================
// Lifecycle API
//=============================================================================

flow_tracker_config_t flow_tracker_default_config(void) {
    flow_tracker_config_t config = {
        .measurement_window_ms = FLOW_TRACKER_MEASUREMENT_WINDOW_MS,
        .latency_histogram_bins = FLOW_TRACKER_LATENCY_BINS,
        .efficiency_warning_threshold = 0.7F,
        .bottleneck_threshold = 0.8F,
        .enable_latency_tracking = true
    };
    return config;
}

flow_tracker_t* flow_tracker_create(void) {
    flow_tracker_config_t config = flow_tracker_default_config();
    return flow_tracker_create_custom(&config);
}

flow_tracker_t* flow_tracker_create_custom(
    const flow_tracker_config_t* config
) {
    if (!config) {
        LOG_ERROR("FlowTracker: NULL config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;
    }

    flow_tracker_t* tracker = (flow_tracker_t*)nimcp_calloc(1, sizeof(flow_tracker_t));
    if (!tracker) {
        LOG_ERROR("FlowTracker: Failed to allocate tracker");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tracker is NULL");

        return NULL;
    }

    tracker->config = *config;

    // Initialize per-path state
    uint64_t now_us = nimcp_time_get_us();
    for (int i = 0; i < FLOW_TRACKER_NUM_PATHS; i++) {
        path_flow_state_t* path = &tracker->paths[i];

        path->measurement_window_start_us = now_us;
        path->last_update_time_us = now_us;
        path->min_latency_us = INFINITY;
        path->max_latency_us = 0.0F;
        path->latency_hist.min_value_us = INFINITY;
        path->latency_hist.max_value_us = 0.0F;

        if (nimcp_mutex_init(&path->mutex, NULL) != NIMCP_SUCCESS) {
            LOG_ERROR("FlowTracker: Failed to init mutex for path %d", i);
            for (int j = 0; j < i; j++) {
                nimcp_mutex_destroy(&tracker->paths[j].mutex);
            }
            nimcp_free(tracker);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "flow_tracker_create_custom: validation failed");
            return NULL;
        }
    }

    tracker->worst_path = PATH_MIDDLEWARE_TO_EXECUTIVE;
    tracker->worst_path_efficiency = 1.0F;
    tracker->last_global_update_us = now_us;

    // Bio-async registration
    tracker->bio_ctx = NULL;
    tracker->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_MIDDLEWARE_FLOW_TRACKER,
            .module_name = "flow_tracker",
            .inbox_capacity = 64,
            .user_data = tracker
        };
        tracker->bio_ctx = bio_router_register_module(&bio_info);
        if (tracker->bio_ctx) {
            tracker->bio_async_enabled = true;
            LOG_INFO("FlowTracker: Bio-async enabled");
        }
    }

    LOG_INFO("FlowTracker: Created (paths=%d, window=%lums)",
             FLOW_TRACKER_NUM_PATHS, config->measurement_window_ms);

    return tracker;
}

void flow_tracker_destroy(flow_tracker_t* tracker) {
    if (!tracker) return;

    LOG_INFO("FlowTracker: Destroying (throughput=%.2f bits/sec)",
             tracker->total_throughput_bits_per_sec);

    // Unregister from bio-async
    if (tracker->bio_async_enabled && tracker->bio_ctx) {
        bio_router_unregister_module(tracker->bio_ctx);
        tracker->bio_ctx = NULL;
        tracker->bio_async_enabled = false;
    }

    for (int i = 0; i < FLOW_TRACKER_NUM_PATHS; i++) {
        nimcp_mutex_destroy(&tracker->paths[i].mutex);
    }

    nimcp_free(tracker);
}

//=============================================================================
// Flow Recording API
//=============================================================================

void flow_tracker_record_flow(
    flow_tracker_t* tracker,
    integration_path_t path,
    float information_bits,
    uint64_t latency_us
) {
    if (!tracker || path >= FLOW_TRACKER_NUM_PATHS) return;

    // Process pending bio-async messages
    if (tracker->bio_async_enabled && tracker->bio_ctx) {
        bio_router_process_inbox(tracker->bio_ctx, 5);
    }

    path_flow_state_t* path_state = &tracker->paths[path];

    nimcp_mutex_lock(&path_state->mutex);

    path_state->total_events++;
    path_state->total_information_bits += information_bits;

    if (tracker->config.enable_latency_tracking && latency_us > 0) {
        path_state->latency_sum_us += latency_us;
        path_state->latency_sq_sum_us += (latency_us * latency_us);

        if (latency_us < path_state->min_latency_us) {
            path_state->min_latency_us = (float)latency_us;
        }
        if (latency_us > path_state->max_latency_us) {
            path_state->max_latency_us = (float)latency_us;
        }

        update_latency_histogram(&path_state->latency_hist, (float)latency_us);
    }

    update_path_metrics(path_state);

    nimcp_mutex_unlock(&path_state->mutex);
}

void flow_tracker_record_filtered_flow(
    flow_tracker_t* tracker,
    integration_path_t path,
    float information_bits
) {
    if (!tracker || path >= FLOW_TRACKER_NUM_PATHS) return;

    path_flow_state_t* path_state = &tracker->paths[path];

    nimcp_mutex_lock(&path_state->mutex);

    path_state->filtered_events++;
    path_state->filtered_information_bits += information_bits;
    path_state->information_loss_bits += information_bits;

    update_path_metrics(path_state);

    nimcp_mutex_unlock(&path_state->mutex);
}

void flow_tracker_record_bottlenecked_flow(
    flow_tracker_t* tracker,
    integration_path_t path,
    float information_bits
) {
    if (!tracker || path >= FLOW_TRACKER_NUM_PATHS) return;

    path_flow_state_t* path_state = &tracker->paths[path];

    nimcp_mutex_lock(&path_state->mutex);

    path_state->bottlenecked_events++;
    path_state->information_loss_bits += information_bits;

    LOG_DEBUG("FlowTracker: Bottleneck on %s (event #%lu)",
              path_names[path], path_state->bottlenecked_events);

    update_path_metrics(path_state);

    nimcp_mutex_unlock(&path_state->mutex);
}

//=============================================================================
// Metrics Access API
//=============================================================================

cross_modal_flow_metrics_t flow_tracker_get_metrics(
    const flow_tracker_t* tracker
) {
    cross_modal_flow_metrics_t metrics = {0};

    if (!tracker) return metrics;

    // Collect per-path metrics
    for (int i = 0; i < FLOW_TRACKER_NUM_PATHS; i++) {
        const path_flow_state_t* path_state = &tracker->paths[i];

        nimcp_mutex_lock((nimcp_mutex_t*)&path_state->mutex);

        path_flow_stats_t* stats = &metrics.paths[i];
        stats->input_rate_bits_per_sec = path_state->input_rate_bits_per_sec;
        stats->output_rate_bits_per_sec = path_state->output_rate_bits_per_sec;
        stats->throughput_bits_per_sec = path_state->throughput_bits_per_sec;
        stats->flow_efficiency = path_state->flow_efficiency;
        stats->bottleneck_severity = path_state->bottleneck_severity;
        stats->channel_capacity_bits_per_sec = path_state->channel_capacity_bits_per_sec;
        stats->capacity_utilization = path_state->capacity_utilization;
        stats->avg_latency_us = path_state->avg_latency_us;
        stats->min_latency_us = path_state->min_latency_us;
        stats->max_latency_us = path_state->max_latency_us;
        stats->p50_latency_us = path_state->p50_latency_us;
        stats->p90_latency_us = path_state->p90_latency_us;
        stats->p99_latency_us = path_state->p99_latency_us;
        stats->stddev_latency_us = path_state->stddev_latency_us;
        stats->total_events = path_state->total_events;
        stats->filtered_events = path_state->filtered_events;
        stats->bottlenecked_events = path_state->bottlenecked_events;
        stats->information_loss_bits = path_state->information_loss_bits;
        stats->loss_percentage = path_state->loss_percentage;
        stats->measurement_window_start_ms = path_state->measurement_window_start_us / 1000;
        stats->last_update_time_ms = path_state->last_update_time_us / 1000;

        nimcp_mutex_unlock((nimcp_mutex_t*)&path_state->mutex);
    }

    // Global metrics
    metrics.total_throughput_bits_per_sec = tracker->total_throughput_bits_per_sec;
    metrics.avg_flow_efficiency = tracker->avg_flow_efficiency;
    metrics.worst_path_efficiency = tracker->worst_path_efficiency;
    metrics.worst_path = tracker->worst_path;
    metrics.any_bottleneck_detected = tracker->any_bottleneck_detected;
    metrics.num_bottlenecked_paths = tracker->num_bottlenecked_paths;
    metrics.measurement_window_ms = tracker->config.measurement_window_ms;
    metrics.last_global_update_ms = tracker->last_global_update_us / 1000;

    return metrics;
}

path_flow_stats_t flow_tracker_get_path_stats(
    const flow_tracker_t* tracker,
    integration_path_t path
) {
    path_flow_stats_t stats = {0};

    if (!tracker || path >= FLOW_TRACKER_NUM_PATHS) return stats;

    cross_modal_flow_metrics_t metrics = flow_tracker_get_metrics(tracker);
    return metrics.paths[path];
}

float flow_tracker_calculate_efficiency(
    const flow_tracker_t* tracker,
    integration_path_t path
) {
    if (!tracker || path >= FLOW_TRACKER_NUM_PATHS) return 0.0F;

    const path_flow_state_t* path_state = &tracker->paths[path];
    return path_state->flow_efficiency;
}

float flow_tracker_get_throughput(
    const flow_tracker_t* tracker,
    integration_path_t path
) {
    if (!tracker || path >= FLOW_TRACKER_NUM_PATHS) return 0.0F;

    const path_flow_state_t* path_state = &tracker->paths[path];
    return path_state->throughput_bits_per_sec;
}

float flow_tracker_get_utilization(
    const flow_tracker_t* tracker,
    integration_path_t path
) {
    if (!tracker || path >= FLOW_TRACKER_NUM_PATHS) return 0.0F;

    const path_flow_state_t* path_state = &tracker->paths[path];
    return path_state->capacity_utilization;
}

float flow_tracker_get_avg_latency(
    const flow_tracker_t* tracker,
    integration_path_t path
) {
    if (!tracker || path >= FLOW_TRACKER_NUM_PATHS) return 0.0F;

    const path_flow_state_t* path_state = &tracker->paths[path];
    return path_state->avg_latency_us;
}

float flow_tracker_get_p99_latency(
    const flow_tracker_t* tracker,
    integration_path_t path
) {
    if (!tracker || path >= FLOW_TRACKER_NUM_PATHS) return 0.0F;

    const path_flow_state_t* path_state = &tracker->paths[path];
    return path_state->p99_latency_us;
}

//=============================================================================
// Analysis API
//=============================================================================

integration_path_t flow_tracker_find_bottleneck(
    const flow_tracker_t* tracker,
    float* efficiency
) {
    if (!tracker) {
        if (efficiency) *efficiency = 1.0F;
        return PATH_MIDDLEWARE_TO_EXECUTIVE;
    }

    float worst_eff = 1.0F;
    integration_path_t worst = PATH_MIDDLEWARE_TO_EXECUTIVE;

    for (int i = 0; i < FLOW_TRACKER_NUM_PATHS; i++) {
        float eff = tracker->paths[i].flow_efficiency;
        if (eff < worst_eff) {
            worst_eff = eff;
            worst = (integration_path_t)i;
        }
    }

    if (efficiency) *efficiency = worst_eff;

    return worst;
}

bool flow_tracker_has_bottleneck(
    const flow_tracker_t* tracker
) {
    if (!tracker) {
        return false;
    }

    for (int i = 0; i < FLOW_TRACKER_NUM_PATHS; i++) {
        if (tracker->paths[i].capacity_utilization > tracker->config.bottleneck_threshold) {
            return true;
        }
    }

    return false;
}

float flow_tracker_get_total_throughput(
    const flow_tracker_t* tracker
) {
    if (!tracker) return 0.0F;

    float total = 0.0F;
    for (int i = 0; i < FLOW_TRACKER_NUM_PATHS; i++) {
        total += tracker->paths[i].throughput_bits_per_sec;
    }

    return total;
}

float flow_tracker_get_avg_efficiency(
    const flow_tracker_t* tracker
) {
    if (!tracker) return 0.0F;

    float sum = 0.0F;
    uint32_t count = 0;

    for (int i = 0; i < FLOW_TRACKER_NUM_PATHS; i++) {
        if (tracker->paths[i].total_events > 0) {
            sum += tracker->paths[i].flow_efficiency;
            count++;
        }
    }

    return count > 0 ? (sum / (float)count) : 0.0F;
}

//=============================================================================
// Utility API
//=============================================================================

void flow_tracker_reset(
    flow_tracker_t* tracker
) {
    if (!tracker) return;

    LOG_INFO("FlowTracker: Resetting all paths");

    uint64_t now_us = nimcp_time_get_us();

    for (int i = 0; i < FLOW_TRACKER_NUM_PATHS; i++) {
        path_flow_state_t* path = &tracker->paths[i];

        nimcp_mutex_lock(&path->mutex);

        /* P1 fix: Reset fields individually instead of memset to preserve
         * the embedded mutex. Copying/memset on a mutex is undefined behavior. */
        path->input_rate_bits_per_sec = 0.0F;
        path->output_rate_bits_per_sec = 0.0F;
        path->throughput_bits_per_sec = 0.0F;
        path->flow_efficiency = 0.0F;
        path->bottleneck_severity = 0.0F;
        path->channel_capacity_bits_per_sec = 0.0F;
        path->capacity_utilization = 0.0F;
        memset(&path->latency_hist, 0, sizeof(path->latency_hist));
        path->avg_latency_us = 0.0F;
        path->min_latency_us = INFINITY;
        path->max_latency_us = 0.0F;
        path->p50_latency_us = 0.0F;
        path->p90_latency_us = 0.0F;
        path->p99_latency_us = 0.0F;
        path->stddev_latency_us = 0.0F;
        path->total_events = 0;
        path->filtered_events = 0;
        path->bottlenecked_events = 0;
        path->information_loss_bits = 0.0F;
        path->loss_percentage = 0.0F;
        path->measurement_window_start_us = now_us;
        path->last_update_time_us = now_us;
        path->total_information_bits = 0.0F;
        path->filtered_information_bits = 0.0F;
        path->latency_sum_us = 0;
        path->latency_sq_sum_us = 0;
        /* mutex is intentionally NOT reset */
        path->latency_hist.min_value_us = INFINITY;
        path->latency_hist.max_value_us = 0.0F;

        nimcp_mutex_unlock(&path->mutex);
    }

    tracker->total_throughput_bits_per_sec = 0.0F;
    tracker->avg_flow_efficiency = 0.0F;
    tracker->worst_path_efficiency = 1.0F;
    tracker->worst_path = PATH_MIDDLEWARE_TO_EXECUTIVE;
    tracker->any_bottleneck_detected = false;
    tracker->num_bottlenecked_paths = 0;
    tracker->last_global_update_us = now_us;
}

const char* flow_tracker_path_to_string(
    integration_path_t path
) {
    if (path >= FLOW_TRACKER_NUM_PATHS) {
        return "Unknown";
    }
    return path_names[path];
}
