/**
 * @file nimcp_temporal_coding.h
 * @brief Temporal coding: Spike timing and inter-spike interval encoding
 *
 * WHAT: Encode information in spike timing patterns and ISI distributions
 * WHY:  Temporal precision carries distinct information from firing rate
 * HOW:  First-spike latency, ISI patterns, spike timing jitter, correlations
 *
 * BIOLOGICAL BASIS:
 * - Sensory neurons encode stimulus onset with first-spike latency (<10ms precision)
 * - Hippocampal place cells use temporal codes during theta oscillations
 * - Auditory system uses microsecond-precise timing for sound localization
 * - ISI patterns encode stimulus features independent of rate
 *
 * @author NIMCP Development Team
 * @date 2025-01-19
 */

#ifndef NIMCP_TEMPORAL_CODING_H
#define NIMCP_TEMPORAL_CODING_H

#include "middleware/encoding/nimcp_rate_coding.h"  // For spike_train_t
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define TEMPORAL_MAX_LATENCY_MS 1000.0f
#define TEMPORAL_MIN_PRECISION_MS 0.1f
#define TEMPORAL_MAX_BINS 100

//=============================================================================
// Core Types
//=============================================================================

/**
 * WHAT: Temporal coding configuration
 * WHY:  Control temporal precision and encoding parameters
 */
typedef struct {
    float latency_window_ms;     /**< Window for first-spike latency */
    float precision_threshold_ms; /**< Timing precision threshold */
    bool encode_isi_patterns;    /**< Encode ISI distribution */
    bool encode_spike_timing;    /**< Encode precise spike times */
    uint32_t num_isi_bins;       /**< Number of ISI histogram bins */
} temporal_coding_config_t;

/**
 * WHAT: Temporal features extracted from spike train
 * WHY:  Compact representation of temporal patterns
 */
typedef struct {
    float first_spike_latency;   /**< Time to first spike (ms) */
    float mean_isi;              /**< Mean inter-spike interval (ms) */
    float isi_std;               /**< ISI standard deviation */
    float isi_cv;                /**< ISI coefficient of variation */
    float* isi_histogram;        /**< ISI distribution (num_bins values) */
    uint32_t num_isi_bins;       /**< Number of histogram bins */
    float spike_timing_precision; /**< Timing jitter (ms) */
    uint32_t num_spikes;         /**< Total spike count */
} temporal_features_t;

/**
 * WHAT: Temporal coding encoder
 */
typedef struct temporal_coding_encoder_struct* temporal_coding_encoder_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

temporal_coding_encoder_t temporal_coding_create(const temporal_coding_config_t* config);
void temporal_coding_destroy(temporal_coding_encoder_t encoder);
temporal_coding_config_t temporal_coding_default_config(void);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * WHAT: Extract temporal features from spike train
 * WHY:  Convert spike times to temporal feature vector
 * HOW:  Calculate latency, ISI stats, timing precision
 */
bool temporal_coding_encode(
    temporal_coding_encoder_t encoder,
    const spike_train_t* spike_train,
    uint64_t ref_time,
    temporal_features_t* features_out
);

/**
 * WHAT: Encode population temporal features
 */
uint32_t temporal_coding_encode_population(
    temporal_coding_encoder_t encoder,
    const spike_train_t* spike_trains,
    uint32_t num_neurons,
    uint64_t ref_time,
    temporal_features_t* features_out
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * WHAT: Generate spike train from temporal features
 */
bool temporal_coding_decode(
    temporal_coding_encoder_t encoder,
    const temporal_features_t* features,
    float duration_ms,
    spike_train_t* spike_train_out
);

//=============================================================================
// Feature Analysis
//=============================================================================

/**
 * WHAT: Calculate temporal correlation between two spike trains
 * WHY:  Measure synchrony and temporal relationships
 */
bool temporal_coding_compute_correlation(
    temporal_coding_encoder_t encoder,
    const spike_train_t* train1,
    const spike_train_t* train2,
    float max_lag_ms,
    float* correlation_out
);

/**
 * WHAT: Compute spike timing jitter
 * WHY:  Measure temporal precision reliability
 */
bool temporal_coding_compute_jitter(
    const spike_train_t* spike_train,
    float expected_isi_ms,
    float* jitter_out
);

//=============================================================================
// Utilities
//=============================================================================

temporal_features_t* temporal_features_create(uint32_t num_isi_bins);
void temporal_features_destroy(temporal_features_t* features);
temporal_features_t* temporal_features_copy(const temporal_features_t* src);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_TEMPORAL_CODING_H
