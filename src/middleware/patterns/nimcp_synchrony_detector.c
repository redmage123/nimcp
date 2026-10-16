#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_synchrony_detector.c - Synchronized Neural Activity Detection
//=============================================================================

#include "middleware/patterns/nimcp_synchrony_detector.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_memory_pool.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/exception/nimcp_exception_macros.h"



#define LOG_MODULE "nimcp_synchrony_detector"
#define LOG_MODULE_ID 0x0528
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(synchrony_detector)

// ============================================================================
// CONSTANTS
// ============================================================================

#define MAX_SPIKES_PER_WINDOW 100000  // Maximum spikes to store per window
#define DEFAULT_WINDOW_10MS 10.0f
#define DEFAULT_WINDOW_100MS 100.0f
#define DEFAULT_WINDOW_1000MS 1000.0f

// ============================================================================
// INTERNAL STRUCTURES
// ============================================================================

/**
 * @brief Spike event record
 */
typedef struct {
    uint32_t neuron_id;
    double timestamp_ms;
} spike_event_t;

/**
 * @brief Sliding window spike buffer
 */
typedef struct {
    spike_event_t* spikes;       // Circular buffer of spikes
    uint32_t capacity;           // Buffer capacity
    uint32_t count;              // Current spike count
    uint32_t head;               // Write position
    double window_size_ms;       // Window duration
    double oldest_time_ms;       // Oldest spike in window
} spike_window_t;

/**
 * @brief Synchrony detector implementation
 */
struct synchrony_detector {
    // Configuration
    uint32_t num_neurons;
    synchrony_detector_config_t config;

    // Multi-scale windows
    spike_window_t windows[SYNCHRONY_MAX_WINDOWS];

    // Per-neuron spike counts (for coincidence detection)
    uint32_t* neuron_spike_counts;
    double* last_spike_times;

    // Memory pools for hot-path allocations (Phase 1.5)
    // Pool for neuron_fired bool arrays in compute_coincidence_rate and detect_critical_events
    memory_pool_t neuron_bool_pool;
    // Pool for spike_counts uint32_t arrays in compute_mean_correlation
    memory_pool_t spike_counts_pool;

    // Statistics
    uint64_t total_spikes;
    uint64_t total_critical_events;
    double sum_synchrony;
    uint64_t synchrony_measurements;
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Initialize spike window
 */
static bool init_spike_window(spike_window_t* window, double size_ms) {
    if (!window) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_spike_window: window is NULL");
        return false;
    }

    window->capacity = MAX_SPIKES_PER_WINDOW;
    window->spikes = (spike_event_t*)nimcp_calloc(window->capacity, sizeof(spike_event_t));
    if (!window->spikes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_spike_window: window->spikes is NULL");
        return false;
    }

    window->count = 0;
    window->head = 0;
    window->window_size_ms = size_ms;
    window->oldest_time_ms = 0.0;

    return true;
}

/**
 * @brief Free spike window resources
 */
static void free_spike_window(spike_window_t* window) {
    if (window && window->spikes) {
        nimcp_free(window->spikes);
        window->spikes = NULL;
    }
}

/**
 * @brief Add spike to window and remove old spikes
 */
static void window_add_spike(spike_window_t* window, uint32_t neuron_id,
                            double timestamp_ms) {
    // Add new spike
    window->spikes[window->head].neuron_id = neuron_id;
    window->spikes[window->head].timestamp_ms = timestamp_ms;
    window->head = (window->head + 1) % window->capacity;

    if (window->count < window->capacity) {
        window->count++;
    }

    // Remove spikes outside window
    double cutoff_time = timestamp_ms - window->window_size_ms;
    uint32_t valid_count = 0;

    for (uint32_t i = 0; i < window->count; i++) {
        uint32_t idx = (window->head + window->capacity - window->count + i) % window->capacity;
        if (window->spikes[idx].timestamp_ms >= cutoff_time) {
            valid_count = window->count - i;
            window->oldest_time_ms = window->spikes[idx].timestamp_ms;
            break;
        }
    }

    window->count = valid_count;
}

/**
 * @brief Compute spike coincidence rate
 *
 * WHAT: Calculate fraction of neurons firing within coincidence window
 * WHY:  Measure population-level synchrony
 * HOW:  Count unique neurons firing within ±coincidence_window of each spike
 */
static float compute_coincidence_rate(synchrony_detector_t* detector,
                                      const spike_window_t* window,
                                      float coincidence_window_ms) {
    if (!detector || !window || window->count == 0 || detector->num_neurons == 0) {
        return 0.0F;
    }

    // Count unique neurons that fired - Phase 1.5 O(1) pool allocation
    bool* neuron_fired = (bool*)memory_pool_acquire(detector->neuron_bool_pool);
    if (!neuron_fired) return 0.0F;
    memset(neuron_fired, 0, detector->num_neurons * sizeof(bool));

    uint32_t neurons_fired = 0;
    for (uint32_t i = 0; i < window->count; i++) {
        uint32_t idx = (window->head + window->capacity - window->count + i) % window->capacity;
        uint32_t nid = window->spikes[idx].neuron_id;
        if (nid < detector->num_neurons && !neuron_fired[nid]) {
            neuron_fired[nid] = true;
            neurons_fired++;
        }
    }

    // Release back to pool (Phase 1.5)
    memory_pool_release(detector->neuron_bool_pool, neuron_fired);

    return (float)neurons_fired / (float)detector->num_neurons;
}

/**
 * @brief Compute mean pairwise correlation
 *
 * WHAT: Average correlation between all neuron pairs
 * WHY:  Overall synchrony measure across population
 * HOW:  Sample-based correlation for efficiency (not all pairs)
 */
static float compute_mean_correlation(synchrony_detector_t* detector,
                                      const spike_window_t* window) {
    if (!detector || !window || window->count < 2 || detector->num_neurons < 2) {
        return 0.0F;
    }

    // Build per-neuron spike lists - Phase 1.5 O(1) pool allocation
    uint32_t* spike_counts = (uint32_t*)memory_pool_acquire(detector->spike_counts_pool);
    if (!spike_counts) return 0.0F;
    memset(spike_counts, 0, detector->num_neurons * sizeof(uint32_t));

    // Count spikes per neuron
    for (uint32_t i = 0; i < window->count; i++) {
        uint32_t idx = (window->head + window->capacity - window->count + i) % window->capacity;
        uint32_t nid = window->spikes[idx].neuron_id;
        if (nid < detector->num_neurons) {
            spike_counts[nid]++;
        }
    }

    // Compute mean and variance
    float mean = (float)window->count / (float)detector->num_neurons;
    float variance = 0.0F;

    for (uint32_t i = 0; i < detector->num_neurons; i++) {
        float diff = (float)spike_counts[i] - mean;
        variance += diff * diff;
    }
    variance /= (float)detector->num_neurons;

    // Release back to pool (Phase 1.5)
    memory_pool_release(detector->spike_counts_pool, spike_counts);

    // Return normalized variance as correlation proxy
    // High variance = low correlation, low variance = high correlation
    if (variance < 1e-6F) return 1.0F;  // Perfect synchrony
    float cv = sqrtf(variance) / (mean + 1e-6F);  // Coefficient of variation
    return 1.0F / (1.0F + cv);  // Map to [0, 1]
}

/**
 * @brief Detect critical events (>threshold population firing)
 */
static uint32_t detect_critical_events(synchrony_detector_t* detector,
                                       const spike_window_t* window,
                                       float threshold,
                                       float coincidence_window_ms) {
    if (!detector || !window || window->count == 0) return 0;

    uint32_t critical_count = 0;

    // Acquire work buffer once before loop - Phase 1.5 O(1) pool allocation
    bool* fired = (bool*)memory_pool_acquire(detector->neuron_bool_pool);
    if (!fired) return 0;

    /* P2-MW-15 fix: Document O(N^2) complexity. N = window->count (number of spikes).
     * For each spike, we check all other spikes for coincidence - this is inherently
     * O(N^2). For large windows (>10K spikes), consider spatial hashing or sorted
     * timestamp binary search. Added early exit on inner loop once threshold is met. */
    for (uint32_t i = 0; i < window->count; i++) {
        uint32_t idx = (window->head + window->capacity - window->count + i) % window->capacity;
        double center_time = window->spikes[idx].timestamp_ms;

        // Clear and reuse buffer (much faster than alloc/free per iteration)
        memset(fired, 0, detector->num_neurons * sizeof(bool));

        uint32_t local_count = 0;
        uint32_t neurons_needed = (uint32_t)(threshold * (float)detector->num_neurons);

        for (uint32_t j = 0; j < window->count; j++) {
            uint32_t jdx = (window->head + window->capacity - window->count + j) % window->capacity;
            double dt = fabs(window->spikes[jdx].timestamp_ms - center_time);

            if (dt <= coincidence_window_ms) {
                uint32_t nid = window->spikes[jdx].neuron_id;
                if (nid < detector->num_neurons && !fired[nid]) {
                    fired[nid] = true;
                    local_count++;
                    /* P2-MW-15 fix: Early exit once threshold is definitely met */
                    if (local_count >= neurons_needed) break;
                }
            }
        }

        float fraction = (float)local_count / (float)detector->num_neurons;
        if (fraction >= threshold) {
            critical_count++;
        }
    }

    // Release back to pool (Phase 1.5)
    memory_pool_release(detector->neuron_bool_pool, fired);

    return critical_count;
}

// ============================================================================
// PUBLIC API
// ============================================================================

synchrony_detector_config_t synchrony_detector_default_config(uint32_t num_neurons) {
    synchrony_detector_config_t config;
    config.num_neurons = num_neurons;
    config.window_sizes_ms[0] = DEFAULT_WINDOW_10MS;
    config.window_sizes_ms[1] = DEFAULT_WINDOW_100MS;
    config.window_sizes_ms[2] = DEFAULT_WINDOW_1000MS;
    config.num_windows = 3;
    config.coincidence_window_ms = SYNCHRONY_COINCIDENCE_WINDOW_MS;
    config.critical_threshold = SYNCHRONY_CRITICAL_THRESHOLD;
    config.high_threshold = SYNCHRONY_HIGH_THRESHOLD;
    config.enable_correlation = true;
    config.enable_coincidence = true;
    config.enable_critical_detection = true;
    return config;
}

synchrony_detector_t* synchrony_detector_create(const synchrony_detector_config_t* config) {
    // Validate input
    if (!config || config->num_neurons == 0 || config->num_neurons > SYNCHRONY_MAX_NEURONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "synchrony_detector_create: config is NULL or num_neurons invalid");
        return NULL;
    }

    if (config->num_windows == 0 || config->num_windows > SYNCHRONY_MAX_WINDOWS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "synchrony_detector_create: num_windows invalid");
        return NULL;
    }

    // Allocate detector
    synchrony_detector_t* detector = (synchrony_detector_t*)nimcp_calloc(1, sizeof(synchrony_detector_t));
    if (!detector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "synchrony_detector_create: failed to allocate detector");
        return NULL;
    }

    // Copy configuration
    detector->num_neurons = config->num_neurons;
    detector->config = *config;

    // Initialize windows
    bool init_success = true;
    for (uint32_t i = 0; i < config->num_windows; i++) {
        if (!init_spike_window(&detector->windows[i], config->window_sizes_ms[i])) {
            init_success = false;
            break;
        }
    }

    if (!init_success) {
        synchrony_detector_destroy(detector);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synchrony_detector_create: init_success is NULL");
        return NULL;
    }

    // Allocate per-neuron tracking
    detector->neuron_spike_counts = (uint32_t*)nimcp_calloc(config->num_neurons, sizeof(uint32_t));
    detector->last_spike_times = (double*)nimcp_calloc(config->num_neurons, sizeof(double));

    if (!detector->neuron_spike_counts || !detector->last_spike_times) {
        synchrony_detector_destroy(detector);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "synchrony_detector_create: required parameter is NULL (detector->neuron_spike_counts, detector->last_spike_times)");
        return NULL;
    }

    // Initialize memory pools for hot-path allocations (Phase 1.5)
    // Pool for neuron_fired bool arrays - 4 blocks for concurrent detection calls
    memory_pool_config_t bool_pool_config = {
        .block_size = config->num_neurons * sizeof(bool),
        .num_blocks = 4,
        .alignment = 16,  // SIMD alignment
        .enable_tracking = false,
        .enable_guard_pages = false
    };
    detector->neuron_bool_pool = memory_pool_create(&bool_pool_config);

    // Pool for spike_counts uint32_t arrays - 2 blocks for correlation computation
    memory_pool_config_t counts_pool_config = {
        .block_size = config->num_neurons * sizeof(uint32_t),
        .num_blocks = 2,
        .alignment = 16,
        .enable_tracking = false,
        .enable_guard_pages = false
    };
    detector->spike_counts_pool = memory_pool_create(&counts_pool_config);

    if (!detector->neuron_bool_pool || !detector->spike_counts_pool) {
        synchrony_detector_destroy(detector);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "synchrony_detector_create: required parameter is NULL (detector->neuron_bool_pool, detector->spike_counts_pool)");
        return NULL;
    }

    // Initialize statistics
    detector->total_spikes = 0;
    detector->total_critical_events = 0;
    detector->sum_synchrony = 0.0;
    detector->synchrony_measurements = 0;

    return detector;
}

void synchrony_detector_destroy(synchrony_detector_t* detector) {
    if (!detector) return;

    // Free windows
    for (uint32_t i = 0; i < detector->config.num_windows; i++) {
        free_spike_window(&detector->windows[i]);
    }

    // Free per-neuron data
    nimcp_free(detector->neuron_spike_counts);
    nimcp_free(detector->last_spike_times);

    // Destroy memory pools (Phase 1.5)
    memory_pool_destroy(detector->neuron_bool_pool);
    memory_pool_destroy(detector->spike_counts_pool);

    // Free detector
    nimcp_free(detector);
}

bool synchrony_detector_add_spike(synchrony_detector_t* detector,
                                   uint32_t neuron_id,
                                   double timestamp_ms) {
    if (!detector || neuron_id >= detector->num_neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "synchrony_detector_destroy: detector is NULL");
        return false;
    }

    // Add to all windows
    for (uint32_t i = 0; i < detector->config.num_windows; i++) {
        window_add_spike(&detector->windows[i], neuron_id, timestamp_ms);
    }

    // Update per-neuron tracking
    detector->neuron_spike_counts[neuron_id]++;
    detector->last_spike_times[neuron_id] = timestamp_ms;
    detector->total_spikes++;

    return true;
}

bool synchrony_detector_detect(synchrony_detector_t* detector,
                                uint32_t window_idx,
                                synchrony_result_t* result) {
    if (!detector || !result || window_idx >= detector->config.num_windows) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synchrony_detector_destroy: required parameter is NULL (detector, result)");
        return false;
    }

    spike_window_t* window = &detector->windows[window_idx];

    // Initialize result
    memset(result, 0, sizeof(synchrony_result_t));
    result->window_duration_ms = window->window_size_ms;
    result->total_neurons = detector->num_neurons;

    // Count neurons that fired - Phase 1.5 O(1) pool allocation
    bool* neuron_fired = (bool*)memory_pool_acquire(detector->neuron_bool_pool);
    if (!neuron_fired) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "synchrony_detector_destroy: neuron_fired is NULL");
        return false;
    }
    memset(neuron_fired, 0, detector->num_neurons * sizeof(bool));

    for (uint32_t i = 0; i < window->count; i++) {
        uint32_t idx = (window->head + window->capacity - window->count + i) % window->capacity;
        uint32_t nid = window->spikes[idx].neuron_id;
        if (nid < detector->num_neurons) {
            neuron_fired[nid] = true;
        }
    }

    uint32_t neurons_firing = 0;
    for (uint32_t i = 0; i < detector->num_neurons; i++) {
        if (neuron_fired[i]) neurons_firing++;
    }
    memory_pool_release(detector->neuron_bool_pool, neuron_fired);

    result->neurons_firing = neurons_firing;

    // Compute coincidence rate
    if (detector->config.enable_coincidence) {
        result->coincidence_rate = compute_coincidence_rate(
            detector, window, detector->config.coincidence_window_ms);
    }

    // Compute mean correlation
    if (detector->config.enable_correlation) {
        result->mean_correlation = compute_mean_correlation(detector, window);
    }

    // Detect critical events
    if (detector->config.enable_critical_detection) {
        result->critical_events = detect_critical_events(
            detector, window, detector->config.critical_threshold,
            detector->config.coincidence_window_ms);

        if (result->critical_events > 0) {
            detector->total_critical_events += result->critical_events;
        }
    }

    // Compute overall synchrony index (weighted combination)
    result->synchrony_index = 0.5F * result->coincidence_rate +
                             0.5F * result->mean_correlation;

    // Set flags
    result->is_synchronized = (result->synchrony_index >= detector->config.high_threshold);
    result->is_critical_event = (result->coincidence_rate >= detector->config.critical_threshold);

    // Update statistics
    detector->sum_synchrony += result->synchrony_index;
    detector->synchrony_measurements++;

    return true;
}

void synchrony_detector_reset(synchrony_detector_t* detector) {
    if (!detector) return;

    // Reset windows
    for (uint32_t i = 0; i < detector->config.num_windows; i++) {
        detector->windows[i].count = 0;
        detector->windows[i].head = 0;
        detector->windows[i].oldest_time_ms = 0.0;
    }

    // Reset per-neuron data
    memset(detector->neuron_spike_counts, 0, detector->num_neurons * sizeof(uint32_t));
    memset(detector->last_spike_times, 0, detector->num_neurons * sizeof(double));

    // Reset running averages but preserve lifetime counters (total_spikes, total_critical_events)
    detector->sum_synchrony = 0.0;
    detector->synchrony_measurements = 0;
}

bool synchrony_detector_get_stats(const synchrony_detector_t* detector,
                                   uint64_t* total_spikes,
                                   uint64_t* total_critical_events,
                                   float* mean_synchrony) {
    if (!detector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synchrony_detector_reset: detector is NULL");
        return false;
    }

    if (total_spikes) *total_spikes = detector->total_spikes;
    if (total_critical_events) *total_critical_events = detector->total_critical_events;

    if (mean_synchrony) {
        *mean_synchrony = (detector->synchrony_measurements > 0) ?
                         (float)(detector->sum_synchrony / detector->synchrony_measurements) : 0.0F;
    }

    return true;
}

float synchrony_detector_compute_correlation(const synchrony_detector_t* detector,
                                              uint32_t neuron_a,
                                              uint32_t neuron_b,
                                              float window_ms) {
    if (!detector || neuron_a >= detector->num_neurons ||
        neuron_b >= detector->num_neurons || neuron_a == neuron_b) {
        return 0.0F;
    }

    /* Find appropriate window (smallest window >= requested size).
     * P3-MW-10 fix: Log warning when no window is large enough and we fall
     * back to the smallest (index 0). This means the requested window_ms
     * exceeds all configured windows. */
    uint32_t window_idx = 0;
    bool found_window = false;
    for (uint32_t i = 0; i < detector->config.num_windows; i++) {
        if (detector->windows[i].window_size_ms >= window_ms) {
            window_idx = i;
            found_window = true;
            break;
        }
    }
    if (!found_window) {
        LOG_WARN(LOG_MODULE, "compute_correlation: requested window %.1f ms exceeds all "
                 "configured windows, falling back to smallest (%.1f ms)",
                 window_ms, detector->windows[0].window_size_ms);
    }

    const spike_window_t* window = &detector->windows[window_idx];

    // Count coincident spikes
    uint32_t coincident = 0;
    uint32_t total_a = 0;
    uint32_t total_b = 0;

    for (uint32_t i = 0; i < window->count; i++) {
        uint32_t idx = (window->head + window->capacity - window->count + i) % window->capacity;
        if (window->spikes[idx].neuron_id == neuron_a) {
            total_a++;
            double time_a = window->spikes[idx].timestamp_ms;

            // Check for coincident spike from neuron_b
            for (uint32_t j = 0; j < window->count; j++) {
                uint32_t jdx = (window->head + window->capacity - window->count + j) % window->capacity;
                if (window->spikes[jdx].neuron_id == neuron_b) {
                    double dt = fabs(window->spikes[jdx].timestamp_ms - time_a);
                    if (dt <= detector->config.coincidence_window_ms) {
                        coincident++;
                        break;
                    }
                }
            }
        } else if (window->spikes[idx].neuron_id == neuron_b) {
            total_b++;
        }
    }

    // Compute correlation (Jaccard-like similarity)
    if (total_a == 0 && total_b == 0) return 0.0F;
    if (total_a == 0 || total_b == 0) return 0.0F;

    /* P3-MW-11 fix: Use max(1, denominator) instead of +1 bias.
     * The +1 systematically underestimates correlation. Using max(1, ...) only
     * protects against the degenerate case where all spikes are coincident
     * (total_a + total_b - coincident == 0). */
    uint32_t denominator = total_a + total_b - coincident;
    if (denominator == 0) denominator = 1;
    return (float)coincident / (float)denominator;
}
