
#include <stddef.h>
#define LOG_MODULE "nimcp_rate_coding"
#define LOG_MODULE_ID 0x0518
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(rate_coding)

/**
 * @file nimcp_rate_coding.c
 * @brief Rate coding implementation
 */

#include "middleware/encoding/nimcp_rate_coding.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "utils/memory/nimcp_memory.h"
#include "utils/rng/nimcp_rand.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/exception/nimcp_exception_macros.h"


//=============================================================================
// Internal Structures
//=============================================================================

/**
 * WHAT: Rate coding encoder internal state
 * WHY:  Maintain smoothing history and configuration
 * HOW:  Store last rate for EMA, configuration parameters
 */
struct rate_coding_encoder_struct {
    rate_coding_config_t config;  /**< Encoder configuration */
    float last_rate;              /**< Last encoded rate for EMA smoothing */
    bool has_last_rate;           /**< Whether last_rate is valid */
    uint32_t encode_count;        /**< Number of encode operations */
};

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * WHAT: Count spikes in time window
 * WHY:  Core operation for rate calculation
 * HOW:  Linear scan through spike array
 */
static uint32_t count_spikes_in_window(
    const spike_train_t* train,
    uint64_t end_time,
    float window_ms
) {
    if (!train || !train->spike_times || train->num_spikes == 0) {
        return 0;
    }

    uint64_t start_time = (uint64_t)fmaxf(0.0F, (float)end_time - window_ms);
    uint32_t count = 0;

    // Count spikes in [start_time, end_time]
    for (uint32_t i = 0; i < train->num_spikes; i++) {
        if (train->spike_times[i] >= start_time &&
            train->spike_times[i] <= end_time) {
            count++;
        }
    }

    return count;
}

/**
 * WHAT: Apply exponential moving average
 * WHY:  Smooth rate estimates over time
 * HOW:  new_rate = alpha * raw_rate + (1 - alpha) * last_rate
 */
static float apply_ema_smoothing(
    float raw_rate,
    float last_rate,
    float alpha
) {
    if (alpha <= 0.0F || alpha >= 1.0F) {
        return raw_rate;  // No smoothing
    }
    return alpha * raw_rate + (1.0F - alpha) * last_rate;
}

/**
 * WHAT: Calculate inter-spike intervals
 * WHY:  Needed for burst detection and CV calculation
 * HOW:  Differences between consecutive spike times
 */
static bool calculate_isi(
    const spike_train_t* train,
    float* isi_out,
    uint32_t* num_isi_out
) {
    if (!train || !isi_out || !num_isi_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calculate_isi: required parameter is NULL (train, isi_out, num_isi_out)");
        return false;
    }

    if (train->num_spikes < 2) {
        *num_isi_out = 0;
        return true;
    }

    uint32_t num_isi = train->num_spikes - 1;
    for (uint32_t i = 0; i < num_isi; i++) {
        isi_out[i] = (float)(train->spike_times[i + 1] - train->spike_times[i]);
    }

    *num_isi_out = num_isi;
    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

rate_coding_encoder_t rate_coding_create(const rate_coding_config_t* config) {
    // Allocate encoder structure
    rate_coding_encoder_t encoder = (rate_coding_encoder_t)nimcp_calloc(
        1, sizeof(struct rate_coding_encoder_struct)
    );
    if (!encoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rate_coding_create: encoder is NULL");
        return NULL;
    }

    // Use provided config or defaults
    if (config) {
        memcpy(&encoder->config, config, sizeof(rate_coding_config_t));
    } else {
        encoder->config = rate_coding_default_config();
    }

    // Validate configuration
    if (encoder->config.window_ms < RATE_CODING_MIN_WINDOW_MS) {
        encoder->config.window_ms = RATE_CODING_MIN_WINDOW_MS;
    }
    if (encoder->config.window_ms > RATE_CODING_MAX_WINDOW_MS) {
        encoder->config.window_ms = RATE_CODING_MAX_WINDOW_MS;
    }
    if (encoder->config.ema_alpha < 0.0F) {
        encoder->config.ema_alpha = 0.0F;
    }
    if (encoder->config.ema_alpha > 1.0F) {
        encoder->config.ema_alpha = 1.0F;
    }

    // Initialize state
    encoder->last_rate = 0.0F;
    encoder->has_last_rate = false;
    encoder->encode_count = 0;

    return encoder;
}

void rate_coding_destroy(rate_coding_encoder_t encoder) {
    if (!encoder) {
        return;
    }
    nimcp_free(encoder);
}

rate_coding_config_t rate_coding_default_config(void) {
    rate_coding_config_t config = {
        .window_ms = RATE_CODING_DEFAULT_WINDOW_MS,
        .ema_alpha = 0.3F,
        .enable_burst_filter = false,
        .burst_threshold_hz = 100.0F,
        .burst_min_isi_ms = 5.0F,
        .adaptive_binning = true
    };
    return config;
}

//=============================================================================
// Encoding Functions
//=============================================================================

bool rate_coding_encode(
    rate_coding_encoder_t encoder,
    const spike_train_t* spike_train,
    uint64_t current_time,
    float* rate_out
) {
    // Guard clauses
    if (!encoder || !spike_train || !rate_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rate_coding_encode: required parameter is NULL (encoder, spike_train, rate_out)");
        return false;
    }

    // Count spikes in window
    uint32_t spike_count = count_spikes_in_window(
        spike_train,
        current_time,
        encoder->config.window_ms
    );

    // Calculate raw firing rate (Hz)
    // Rate = spikes / (window_seconds)
    float window_sec = encoder->config.window_ms / 1000.0F;
    float raw_rate = (float)spike_count / window_sec;

    // Apply EMA smoothing if enabled and we have previous rate
    float smoothed_rate = raw_rate;
    if (encoder->has_last_rate && encoder->config.ema_alpha > 0.0F) {
        smoothed_rate = apply_ema_smoothing(
            raw_rate,
            encoder->last_rate,
            encoder->config.ema_alpha
        );
    }

    // Update encoder state
    encoder->last_rate = smoothed_rate;
    encoder->has_last_rate = true;
    encoder->encode_count++;

    *rate_out = smoothed_rate;
    return true;
}

uint32_t rate_coding_encode_population(
    rate_coding_encoder_t encoder,
    const spike_train_t* spike_trains,
    uint32_t num_neurons,
    uint64_t current_time,
    float* rates_out
) {
    // Guard clauses
    if (!encoder || !spike_trains || !rates_out || num_neurons == 0) {
        return 0;
    }

    uint32_t success_count = 0;

    // Encode each neuron
    for (uint32_t i = 0; i < num_neurons; i++) {
        float rate = 0.0F;
        if (rate_coding_encode(encoder, &spike_trains[i], current_time, &rate)) {
            rates_out[i] = rate;
            success_count++;
        } else {
            rates_out[i] = 0.0F;
        }
    }

    return success_count;
}

uint32_t rate_coding_encode_multiscale(
    rate_coding_encoder_t encoder,
    const spike_train_t* spike_train,
    uint64_t current_time,
    const float* windows_ms,
    uint32_t num_windows,
    float* rates_out
) {
    // Guard clauses
    if (!encoder || !spike_train || !windows_ms || !rates_out || num_windows == 0) {
        return 0;
    }

    uint32_t success_count = 0;

    // Calculate rate for each window size
    for (uint32_t i = 0; i < num_windows; i++) {
        // Validate window size
        if (windows_ms[i] < RATE_CODING_MIN_WINDOW_MS ||
            windows_ms[i] > RATE_CODING_MAX_WINDOW_MS) {
            rates_out[i] = 0.0F;
            continue;
        }

        // Count spikes in this window
        uint32_t spike_count = count_spikes_in_window(
            spike_train,
            current_time,
            windows_ms[i]
        );

        // Calculate rate
        float window_sec = windows_ms[i] / 1000.0F;
        rates_out[i] = (float)spike_count / window_sec;
        success_count++;
    }

    return success_count;
}

//=============================================================================
// Decoding Functions
//=============================================================================

bool rate_coding_decode(
    rate_coding_encoder_t encoder,
    float rate_hz,
    float duration_ms,
    bool use_poisson,
    spike_train_t* spike_train_out
) {
    // Guard clauses
    if (!encoder || !spike_train_out || rate_hz < 0.0F || duration_ms <= 0.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rate_coding_decode: required parameter is NULL (encoder, spike_train_out)");
        return false;
    }

    // Clear output spike train
    rate_coding_spike_train_clear(spike_train_out);

    // Handle zero rate case
    if (rate_hz == 0.0F) {
        return true;
    }

    if (use_poisson) {
        // Poisson spike generation
        // Time step = 1ms for reasonable resolution
        float dt_ms = 1.0F;
        float prob_spike = rate_hz * (dt_ms / 1000.0F);  // Probability per ms

        // Generate spikes
        for (float t = 0.0F; t < duration_ms; t += dt_ms) {
            float r = nimcp_rand_uniform();
            if (r < prob_spike) {
                uint64_t spike_time = (uint64_t)t;
                if (!spike_train_add_spike(spike_train_out, spike_time)) {
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rate_coding_decode: spike_train_add_spike is NULL");
                    return false;
                }
            }
        }
    } else {
        // Regular spike train generation
        if (rate_hz > 0.0F) {
            float isi_ms = 1000.0F / rate_hz;  // Inter-spike interval

            // Generate spikes at regular intervals
            for (float t = 0.0F; t < duration_ms; t += isi_ms) {
                uint64_t spike_time = (uint64_t)t;
                if (!spike_train_add_spike(spike_train_out, spike_time)) {
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rate_coding_decode: spike_train_add_spike is NULL");
                    return false;
                }
            }
        }
    }

    // Set spike train metadata
    spike_train_out->start_time = 0;
    spike_train_out->end_time = (uint64_t)duration_ms;

    return true;
}

uint32_t rate_coding_decode_population(
    rate_coding_encoder_t encoder,
    const float* rates_hz,
    uint32_t num_neurons,
    float duration_ms,
    bool use_poisson,
    spike_train_t* spike_trains_out
) {
    // Guard clauses
    if (!encoder || !rates_hz || !spike_trains_out || num_neurons == 0) {
        return 0;
    }

    uint32_t success_count = 0;

    // Decode each neuron
    for (uint32_t i = 0; i < num_neurons; i++) {
        if (rate_coding_decode(
            encoder,
            rates_hz[i],
            duration_ms,
            use_poisson,
            &spike_trains_out[i]
        )) {
            success_count++;
        }
    }

    return success_count;
}

//=============================================================================
// Advanced Features
//=============================================================================

bool rate_coding_detect_bursts(
    rate_coding_encoder_t encoder,
    const spike_train_t* spike_train,
    uint32_t* burst_count_out,
    float* burst_rate_out,
    float* tonic_rate_out
) {
    // Guard clauses
    if (!encoder || !spike_train || !burst_count_out ||
        !burst_rate_out || !tonic_rate_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rate_coding_detect_bursts: operation failed");
        return false;
    }

    // Initialize outputs
    *burst_count_out = 0;
    *burst_rate_out = 0.0F;
    *tonic_rate_out = 0.0F;

    // Need at least 3 spikes for burst detection
    if (spike_train->num_spikes < 3) {
        return true;
    }

    // Calculate ISIs
    float* isi = (float*)nimcp_malloc((spike_train->num_spikes - 1) * sizeof(float));
    if (!isi) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rate_coding_detect_bursts: isi is NULL");
        return false;
    }

    uint32_t num_isi = 0;
    if (!calculate_isi(spike_train, isi, &num_isi)) {
        nimcp_free(isi);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rate_coding_detect_bursts: calculate_isi is NULL");
        return false;
    }

    // Detect bursts: consecutive spikes with ISI < threshold
    uint32_t burst_count = 0;
    uint32_t burst_spike_count = 0;
    uint32_t tonic_spike_count = 0;

    bool in_burst = false;
    uint32_t current_burst_size = 0;

    for (uint32_t i = 0; i < num_isi; i++) {
        if (isi[i] < encoder->config.burst_min_isi_ms) {
            if (!in_burst) {
                // Start of new burst
                in_burst = true;
                current_burst_size = 2;  // Current spike + next spike
            } else {
                // Continue burst
                current_burst_size++;
            }
        } else {
            if (in_burst) {
                // End of burst
                if (current_burst_size >= 3) {
                    burst_count++;
                    burst_spike_count += current_burst_size;
                } else {
                    tonic_spike_count += current_burst_size;
                }
                in_burst = false;
                current_burst_size = 0;
            }
            tonic_spike_count++;  // This spike is tonic
        }
    }

    // Handle final burst
    if (in_burst && current_burst_size >= 3) {
        burst_count++;
        burst_spike_count += current_burst_size;
    } else if (in_burst) {
        tonic_spike_count += current_burst_size;
    }

    // Calculate rates
    float total_duration_sec = 0.0F;
    if (spike_train->num_spikes > 0) {
        total_duration_sec = (float)(
            spike_train->spike_times[spike_train->num_spikes - 1] -
            spike_train->spike_times[0]
        ) / 1000.0F;
    }

    if (total_duration_sec > 0.0F) {
        *burst_rate_out = (float)burst_spike_count / total_duration_sec;
        *tonic_rate_out = (float)tonic_spike_count / total_duration_sec;
    }

    *burst_count_out = burst_count;

    nimcp_free(isi);
    return true;
}

bool rate_coding_instantaneous_rate(
    rate_coding_encoder_t encoder,
    const spike_train_t* spike_train,
    uint64_t time_ms,
    float kernel_width_ms,
    float* inst_rate_out
) {
    // Guard clauses
    if (!encoder || !spike_train || !inst_rate_out || kernel_width_ms <= 0.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rate_coding_instantaneous_rate: required parameter is NULL (encoder, spike_train, inst_rate_out)");
        return false;
    }

    // Gaussian kernel density estimation
    // rate(t) = sum_i K((t - t_i) / h) where K is Gaussian kernel

    float sum = 0.0F;
    float sigma = kernel_width_ms / 2.355F;  // FWHM to sigma conversion
    float two_sigma_sq = 2.0F * sigma * sigma;
    float norm_factor = 1.0F / (sqrtf(2.0F * M_PI) * sigma);

    for (uint32_t i = 0; i < spike_train->num_spikes; i++) {
        float dt = (float)((int64_t)time_ms - (int64_t)spike_train->spike_times[i]);
        float kernel_val = norm_factor * expf(-(dt * dt) / two_sigma_sq);
        sum += kernel_val;
    }

    // Convert from density to rate (Hz)
    *inst_rate_out = sum * 1000.0F;  // Convert ms^-1 to Hz
    return true;
}

//=============================================================================
// Spike Train Utilities
//=============================================================================

spike_train_t* rate_coding_spike_train_create(uint32_t capacity) {
    // Guard clause
    if (capacity == 0 || capacity > RATE_CODING_MAX_SPIKE_HISTORY) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rate_coding_spike_train_create: capacity is 0 or exceeds max");
        return NULL;
    }

    spike_train_t* train = (spike_train_t*)nimcp_calloc(1, sizeof(spike_train_t));
    if (!train) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rate_coding_spike_train_create: failed to allocate train");
        return NULL;
    }

    train->spike_times = (uint64_t*)nimcp_calloc(capacity, sizeof(uint64_t));
    if (!train->spike_times) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rate_coding_spike_train_create: failed to allocate spike_times");
        nimcp_free(train);
        return NULL;
    }

    train->capacity = capacity;
    train->num_spikes = 0;
    train->start_time = 0;
    train->end_time = 0;

    return train;
}

void rate_coding_spike_train_destroy(spike_train_t* train) {
    if (!train) {
        return;
    }
    if (train->spike_times) {
        nimcp_free(train->spike_times);
    }
    nimcp_free(train);
}

bool spike_train_add_spike(spike_train_t* train, uint64_t spike_time) {
    // Guard clause
    if (!train) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spike_train_add_spike: train is NULL");
        return false;
    }

    // Resize if needed
    if (train->num_spikes >= train->capacity) {
        uint32_t new_capacity = train->capacity * 2;
        if (new_capacity > RATE_CODING_MAX_SPIKE_HISTORY) {
            new_capacity = RATE_CODING_MAX_SPIKE_HISTORY;
        }
        if (train->num_spikes >= new_capacity) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "spike_train_add_spike: capacity exceeded");
            return false;  // Already at max capacity
        }

        uint64_t* new_array = (uint64_t*)nimcp_realloc(
            train->spike_times,
            new_capacity * sizeof(uint64_t)
        );
        if (!new_array) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "spike_train_add_spike: new_array is NULL");
            return false;
        }

        train->spike_times = new_array;
        train->capacity = new_capacity;
    }

    // Add spike
    train->spike_times[train->num_spikes] = spike_time;
    train->num_spikes++;

    // Update time window
    if (train->num_spikes == 1) {
        train->start_time = spike_time;
        train->end_time = spike_time;
    } else {
        if (spike_time < train->start_time) {
            train->start_time = spike_time;
        }
        if (spike_time > train->end_time) {
            train->end_time = spike_time;
        }
    }

    return true;
}

void rate_coding_spike_train_clear(spike_train_t* train) {
    if (!train) {
        return;
    }
    train->num_spikes = 0;
    train->start_time = 0;
    train->end_time = 0;
}

spike_train_t* spike_train_copy(const spike_train_t* src) {
    // Guard clause
    if (!src) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spike_train_copy: src is NULL");
        return NULL;
    }

    spike_train_t* copy = rate_coding_spike_train_create(src->capacity);
    if (!copy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "spike_train_copy: copy is NULL");
        return NULL;
    }

    // Copy spike times
    memcpy(copy->spike_times, src->spike_times, src->num_spikes * sizeof(uint64_t));
    copy->num_spikes = src->num_spikes;
    copy->start_time = src->start_time;
    copy->end_time = src->end_time;

    return copy;
}

//=============================================================================
// Statistics
//=============================================================================

bool rate_coding_compute_cv(const spike_train_t* spike_train, float* cv_out) {
    // Guard clauses
    if (!spike_train || !cv_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rate_coding_compute_cv: required parameter is NULL (spike_train, cv_out)");
        return false;
    }

    // Need at least 2 spikes
    if (spike_train->num_spikes < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rate_coding_compute_cv: validation failed");
        return false;
    }

    // Calculate ISIs
    uint32_t num_isi = spike_train->num_spikes - 1;
    float* isi = (float*)nimcp_malloc(num_isi * sizeof(float));
    if (!isi) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rate_coding_compute_cv: isi is NULL");
        return false;
    }

    uint32_t actual_num_isi = 0;
    if (!calculate_isi(spike_train, isi, &actual_num_isi)) {
        nimcp_free(isi);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rate_coding_compute_cv: calculate_isi is NULL");
        return false;
    }

    // Calculate mean ISI
    float mean_isi = 0.0F;
    for (uint32_t i = 0; i < num_isi; i++) {
        mean_isi += isi[i];
    }
    mean_isi /= (float)num_isi;

    // Calculate standard deviation
    float variance = 0.0F;
    for (uint32_t i = 0; i < num_isi; i++) {
        float diff = isi[i] - mean_isi;
        variance += diff * diff;
    }
    variance /= (float)num_isi;
    float std_dev = sqrtf(variance);

    // Calculate CV
    if (mean_isi > 0.0F) {
        *cv_out = std_dev / mean_isi;
    } else {
        *cv_out = 0.0F;
    }

    nimcp_free(isi);
    return true;
}

bool rate_coding_compute_fano_factor(
    const spike_train_t* spike_trains,
    uint32_t num_trials,
    float window_ms,
    float* fano_out
) {
    // Guard clauses
    if (!spike_trains || !fano_out || num_trials == 0 || window_ms <= 0.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rate_coding_compute_fano_factor: required parameter is NULL (spike_trains, fano_out)");
        return false;
    }

    // Count spikes in window for each trial
    uint32_t* spike_counts = (uint32_t*)nimcp_calloc(num_trials, sizeof(uint32_t));
    if (!spike_counts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rate_coding_compute_fano_factor: spike_counts is NULL");
        return false;
    }

    for (uint32_t i = 0; i < num_trials; i++) {
        spike_counts[i] = count_spikes_in_window(
            &spike_trains[i],
            spike_trains[i].end_time,
            window_ms
        );
    }

    // Calculate mean spike count
    float mean_count = 0.0F;
    for (uint32_t i = 0; i < num_trials; i++) {
        mean_count += (float)spike_counts[i];
    }
    mean_count /= (float)num_trials;

    // Calculate variance
    float variance = 0.0F;
    for (uint32_t i = 0; i < num_trials; i++) {
        float diff = (float)spike_counts[i] - mean_count;
        variance += diff * diff;
    }
    variance /= (float)num_trials;

    // Calculate Fano factor
    if (mean_count > 0.0F) {
        *fano_out = variance / mean_count;
    } else {
        *fano_out = 0.0F;
    }

    nimcp_free(spike_counts);
    return true;
}
