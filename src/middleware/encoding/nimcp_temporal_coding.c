
#include <stddef.h>
#define LOG_MODULE "nimcp_temporal_coding"
#define LOG_MODULE_ID 0x0519
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(temporal_coding)

/**
 * @file nimcp_temporal_coding.c
 * @brief Temporal coding implementation
 */

#include "middleware/encoding/nimcp_temporal_coding.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "utils/memory/nimcp_memory.h"
#include "utils/rng/nimcp_rand.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/exception/nimcp_exception_macros.h"

// Use rate_coding_spike_train_clear (not GPU spike_train_clear which has different struct layout)
extern void rate_coding_spike_train_clear(spike_train_t* train);
#define spike_train_clear rate_coding_spike_train_clear


//=============================================================================
// Internal Structures
//=============================================================================

struct temporal_coding_encoder_struct {
    temporal_coding_config_t config;
    uint32_t encode_count;
};

//=============================================================================
// Lifecycle Functions
//=============================================================================

temporal_coding_encoder_t temporal_coding_create(const temporal_coding_config_t* config) {
    temporal_coding_encoder_t encoder = (temporal_coding_encoder_t)nimcp_calloc(
        1, sizeof(struct temporal_coding_encoder_struct)
    );
    if (!encoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "temporal_coding_create: encoder is NULL");
        return NULL;
    }

    if (config) {
        memcpy(&encoder->config, config, sizeof(temporal_coding_config_t));
    } else {
        encoder->config = temporal_coding_default_config();
    }

    // Validate configuration
    if (encoder->config.num_isi_bins == 0) {
        encoder->config.num_isi_bins = 20;
    }
    if (encoder->config.num_isi_bins > TEMPORAL_MAX_BINS) {
        encoder->config.num_isi_bins = TEMPORAL_MAX_BINS;
    }

    encoder->encode_count = 0;
    return encoder;
}

void temporal_coding_destroy(temporal_coding_encoder_t encoder) {
    if (!encoder) {
        return;
    }
    nimcp_free(encoder);
}

temporal_coding_config_t temporal_coding_default_config(void) {
    temporal_coding_config_t config = {
        .latency_window_ms = 100.0F,
        .precision_threshold_ms = 1.0F,
        .encode_isi_patterns = true,
        .encode_spike_timing = true,
        .num_isi_bins = 20
    };
    return config;
}

//=============================================================================
// Encoding Functions
//=============================================================================

bool temporal_coding_encode(
    temporal_coding_encoder_t encoder,
    const spike_train_t* spike_train,
    uint64_t ref_time,
    temporal_features_t* features_out
) {
    if (!encoder || !spike_train || !features_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "temporal_coding_encode: required parameter is NULL (encoder, spike_train, features_out)");
        return false;
    }

    // Initialize features
    features_out->num_spikes = spike_train->num_spikes;
    features_out->first_spike_latency = TEMPORAL_MAX_LATENCY_MS;
    features_out->mean_isi = 0.0F;
    features_out->isi_std = 0.0F;
    features_out->isi_cv = 0.0F;
    features_out->spike_timing_precision = 0.0F;

    // Handle empty spike train
    if (spike_train->num_spikes == 0) {
        return true;
    }

    // Calculate first-spike latency
    for (uint32_t i = 0; i < spike_train->num_spikes; i++) {
        if (spike_train->spike_times[i] >= ref_time) {
            features_out->first_spike_latency =
                (float)(spike_train->spike_times[i] - ref_time);
            break;
        }
    }

    // Need at least 2 spikes for ISI calculations
    if (spike_train->num_spikes < 2) {
        return true;
    }

    // Calculate ISI statistics
    uint32_t num_isi = spike_train->num_spikes - 1;
    float* isi = (float*)nimcp_malloc(num_isi * sizeof(float));
    if (!isi) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "temporal_coding_encode: isi is NULL");
        return false;
    }

    float sum_isi = 0.0F;
    for (uint32_t i = 0; i < num_isi; i++) {
        isi[i] = (float)(spike_train->spike_times[i + 1] - spike_train->spike_times[i]);
        sum_isi += isi[i];
    }

    features_out->mean_isi = sum_isi / (float)num_isi;

    // Calculate ISI variance and standard deviation
    float sum_sq_diff = 0.0F;
    for (uint32_t i = 0; i < num_isi; i++) {
        float diff = isi[i] - features_out->mean_isi;
        sum_sq_diff += diff * diff;
    }
    float variance = sum_sq_diff / (float)num_isi;
    features_out->isi_std = sqrtf(variance);

    // Calculate CV
    if (features_out->mean_isi > 0.0F) {
        features_out->isi_cv = features_out->isi_std / features_out->mean_isi;
    }

    // Build ISI histogram if enabled
    if (encoder->config.encode_isi_patterns && features_out->isi_histogram) {
        // Find min/max ISI for binning
        float min_isi = isi[0];
        float max_isi = isi[0];
        for (uint32_t i = 1; i < num_isi; i++) {
            if (isi[i] < min_isi) min_isi = isi[i];
            if (isi[i] > max_isi) max_isi = isi[i];
        }

        float bin_width = (max_isi - min_isi) / (float)features_out->num_isi_bins;
        if (bin_width > 0.0F) {
            // Initialize histogram
            memset(features_out->isi_histogram, 0,
                   features_out->num_isi_bins * sizeof(float));

            // Fill histogram
            for (uint32_t i = 0; i < num_isi; i++) {
                uint32_t bin = (uint32_t)((isi[i] - min_isi) / bin_width);
                if (bin >= features_out->num_isi_bins) {
                    bin = features_out->num_isi_bins - 1;
                }
                features_out->isi_histogram[bin] += 1.0F;
            }

            // Normalize histogram
            for (uint32_t i = 0; i < features_out->num_isi_bins; i++) {
                features_out->isi_histogram[i] /= (float)num_isi;
            }
        }
    }

    // Calculate spike timing precision (jitter)
    features_out->spike_timing_precision = features_out->isi_std;

    nimcp_free(isi);
    encoder->encode_count++;
    return true;
}

uint32_t temporal_coding_encode_population(
    temporal_coding_encoder_t encoder,
    const spike_train_t* spike_trains,
    uint32_t num_neurons,
    uint64_t ref_time,
    temporal_features_t* features_out
) {
    if (!encoder || !spike_trains || !features_out || num_neurons == 0) {
        return 0;
    }

    uint32_t success_count = 0;
    for (uint32_t i = 0; i < num_neurons; i++) {
        if (temporal_coding_encode(encoder, &spike_trains[i], ref_time, &features_out[i])) {
            success_count++;
        }
    }

    return success_count;
}

//=============================================================================
// Decoding Functions
//=============================================================================

bool temporal_coding_decode(
    temporal_coding_encoder_t encoder,
    const temporal_features_t* features,
    float duration_ms,
    spike_train_t* spike_train_out
) {
    if (!encoder || !features || !spike_train_out || duration_ms <= 0.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "temporal_coding_decode: required parameter is NULL (encoder, features, spike_train_out)");
        return false;
    }

    spike_train_clear(spike_train_out);

    // Generate spikes with specified ISI distribution
    if (features->mean_isi <= 0.0F) {
        return true;  // No spikes to generate
    }

    float current_time = features->first_spike_latency;
    while (current_time < duration_ms) {
        // Add spike
        if (!spike_train_add_spike(spike_train_out, (uint64_t)current_time)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_coding_decode: spike_train_add_spike is NULL");
            return false;
        }

        // Generate next ISI using Gaussian distribution
        float next_isi = features->mean_isi;
        if (features->isi_std > 0.0F) {
            // Add Gaussian noise scaled by std dev
            float z = nimcp_rand_uniform() * 2.0F - 1.0F;  // Approximate
            next_isi += z * features->isi_std;
            if (next_isi < 0.1F) next_isi = 0.1F;  // Prevent negative ISI
        }

        current_time += next_isi;
    }

    spike_train_out->start_time = 0;
    spike_train_out->end_time = (uint64_t)duration_ms;
    return true;
}

//=============================================================================
// Feature Analysis
//=============================================================================

bool temporal_coding_compute_correlation(
    temporal_coding_encoder_t encoder,
    const spike_train_t* train1,
    const spike_train_t* train2,
    float max_lag_ms,
    float* correlation_out
) {
    if (!encoder || !train1 || !train2 || !correlation_out || max_lag_ms <= 0.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "temporal_coding_compute_correlation: required parameter is NULL (encoder, train1, train2, correlation_out)");
        return false;
    }

    // Cross-correlation using coincidence detection
    uint32_t coincidence_count = 0;
    uint32_t total_comparisons = 0;

    for (uint32_t i = 0; i < train1->num_spikes; i++) {
        for (uint32_t j = 0; j < train2->num_spikes; j++) {
            float time_diff = fabs(
                (float)((int64_t)train1->spike_times[i] - (int64_t)train2->spike_times[j])
            );

            if (time_diff <= max_lag_ms) {
                coincidence_count++;
            }
            total_comparisons++;
        }
    }

    if (total_comparisons > 0) {
        *correlation_out = (float)coincidence_count / (float)total_comparisons;
    } else {
        *correlation_out = 0.0F;
    }

    return true;
}

bool temporal_coding_compute_jitter(
    const spike_train_t* spike_train,
    float expected_isi_ms,
    float* jitter_out
) {
    if (!spike_train || !jitter_out || expected_isi_ms <= 0.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "temporal_coding_compute_jitter: required parameter is NULL (spike_train, jitter_out)");
        return false;
    }

    if (spike_train->num_spikes < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_coding_compute_jitter: validation failed");
        return false;
    }

    // Calculate deviation from expected ISI
    uint32_t num_isi = spike_train->num_spikes - 1;
    float sum_sq_dev = 0.0F;

    for (uint32_t i = 0; i < num_isi; i++) {
        float isi = (float)(spike_train->spike_times[i + 1] - spike_train->spike_times[i]);
        float dev = isi - expected_isi_ms;
        sum_sq_dev += dev * dev;
    }

    *jitter_out = sqrtf(sum_sq_dev / (float)num_isi);
    return true;
}

//=============================================================================
// Utilities
//=============================================================================

temporal_features_t* temporal_features_create(uint32_t num_isi_bins) {
    if (num_isi_bins == 0 || num_isi_bins > TEMPORAL_MAX_BINS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_features_create: num_isi_bins is 0 or exceeds max");
        return NULL;
    }

    temporal_features_t* features = (temporal_features_t*)nimcp_calloc(
        1, sizeof(temporal_features_t)
    );
    if (!features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "temporal_features_create: failed to allocate features");
        return NULL;
    }

    features->isi_histogram = (float*)nimcp_calloc(num_isi_bins, sizeof(float));
    if (!features->isi_histogram) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "temporal_features_create: failed to allocate histogram");
        nimcp_free(features);
        return NULL;
    }

    features->num_isi_bins = num_isi_bins;
    return features;
}

void temporal_features_destroy(temporal_features_t* features) {
    if (!features) {
        return;
    }
    if (features->isi_histogram) {
        nimcp_free(features->isi_histogram);
    }
    nimcp_free(features);
}

temporal_features_t* temporal_features_copy(const temporal_features_t* src) {
    if (!src) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "temporal_features_copy: src is NULL");
        return NULL;
    }

    temporal_features_t* copy = temporal_features_create(src->num_isi_bins);
    if (!copy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "temporal_features_copy: copy is NULL");
        return NULL;
    }

    copy->first_spike_latency = src->first_spike_latency;
    copy->mean_isi = src->mean_isi;
    copy->isi_std = src->isi_std;
    copy->isi_cv = src->isi_cv;
    copy->spike_timing_precision = src->spike_timing_precision;
    copy->num_spikes = src->num_spikes;

    if (src->isi_histogram && copy->isi_histogram) {
        memcpy(copy->isi_histogram, src->isi_histogram,
               src->num_isi_bins * sizeof(float));
    }

    return copy;
}
