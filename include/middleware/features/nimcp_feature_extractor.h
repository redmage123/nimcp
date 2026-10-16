//=============================================================================
// nimcp_feature_extractor.h - Neural Feature Extraction Engine
//=============================================================================

#ifndef NIMCP_FEATURE_EXTRACTOR_H
#define NIMCP_FEATURE_EXTRACTOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_feature_extractor.h
 * @brief Extract multi-dimensional features from neural spike data
 *
 * WHAT: Comprehensive feature extraction from population spike trains
 * WHY:  Convert raw spike data to interpretable neuroscience metrics
 * HOW:  Compute rate, temporal, population, oscillation, and information features
 *
 * BIOLOGICAL BASIS:
 * - Firing rate: Primary neural code for many brain regions
 * - ISI statistics: Regularity and burst patterns
 * - Population synchrony: Coordinated neural processing
 * - Oscillations: Brain state and cognitive mode indicators
 * - Entropy: Information content and coding efficiency
 *
 * FEATURES EXTRACTED:
 * 1. Rate-based: Mean firing rate, population rate std
 * 2. Temporal: Mean ISI, CV of ISI, spike timing precision
 * 3. Population: Synchrony index, burst index, Fano factor
 * 4. Oscillations: Delta, theta, alpha, beta, gamma power
 * 5. Information: Spike entropy (Shannon)
 *
 * @author NIMCP Development Team
 * @date 2025-01-19
 */

//=============================================================================
// Constants
//=============================================================================

#define FEATURE_EXTRACTOR_MAX_NEURONS 10000
#define FEATURE_EXTRACTOR_MIN_WINDOW_MS 10.0f
#define FEATURE_EXTRACTOR_MAX_WINDOW_MS 5000.0f
#define FEATURE_EXTRACTOR_NUM_BANDS 5

// Oscillation band ranges (Hz)
#define FEATURE_DELTA_MIN_HZ 0.5f
#define FEATURE_DELTA_MAX_HZ 4.0f
#define FEATURE_THETA_MIN_HZ 4.0f
#define FEATURE_THETA_MAX_HZ 8.0f
#define FEATURE_ALPHA_MIN_HZ 8.0f
#define FEATURE_ALPHA_MAX_HZ 13.0f
#define FEATURE_BETA_MIN_HZ 13.0f
#define FEATURE_BETA_MAX_HZ 30.0f
#define FEATURE_GAMMA_MIN_HZ 30.0f
#define FEATURE_GAMMA_MAX_HZ 100.0f

//=============================================================================
// Core Types
//=============================================================================

/**
 * WHAT: Comprehensive neural feature vector
 * WHY:  Unified representation for downstream processing
 * HOW:  Aggregates all feature types into single struct
 */
typedef struct {
    // Rate-based features
    float mean_firing_rate;       /**< Population average rate (Hz) */
    float population_rate_std;    /**< Population rate std dev (Hz) */

    // Temporal features
    float mean_isi;               /**< Mean inter-spike interval (ms) */
    float isi_cv;                 /**< ISI coefficient of variation */
    float spike_timing_precision; /**< Timing jitter (ms) */

    // Population features
    float synchrony_index;        /**< Population synchrony [0, 1] */
    float burst_index;            /**< Proportion of spikes in bursts [0, 1] */
    float fano_factor;            /**< Variance/mean spike count */

    // Oscillation power (band-specific)
    float delta_power;   /**< 0.5-4 Hz power */
    float theta_power;   /**< 4-8 Hz power */
    float alpha_power;   /**< 8-13 Hz power */
    float beta_power;    /**< 13-30 Hz power */
    float gamma_power;   /**< 30-100 Hz power */

    // Information theory
    float spike_entropy;  /**< Shannon entropy (bits) */

    // Metadata
    uint64_t timestamp;   /**< When features were extracted (ms) */
    bool valid;           /**< Whether features are current */
} middleware_features_t;

/**
 * WHAT: Spike data input for feature extraction
 * WHY:  Flexible input format supporting multiple encodings
 * HOW:  Array of spike times per neuron
 */
typedef struct {
    uint64_t** spike_times;  /**< Array of spike time arrays (one per neuron) */
    uint32_t* spike_counts;  /**< Number of spikes per neuron */
    uint32_t num_neurons;    /**< Total neurons in population */
    uint64_t start_time;     /**< Window start time (ms) */
    uint64_t end_time;       /**< Window end time (ms) */
} spike_data_t;

/**
 * WHAT: Feature extractor configuration
 * WHY:  Customize feature extraction for different use cases
 * HOW:  Flags and parameters for each feature type
 */
typedef struct {
    float window_ms;                /**< Analysis window duration (ms) */
    float synchrony_window_ms;      /**< Coincidence window for synchrony (ms) */
    float burst_isi_threshold_ms;   /**< ISI threshold for burst detection (ms) */
    uint32_t min_burst_spikes;      /**< Minimum spikes per burst */
    uint32_t entropy_bins;          /**< Bins for entropy calculation */
    bool compute_oscillations;      /**< Enable oscillation power analysis */
    bool compute_entropy;           /**< Enable entropy calculation */
    bool compute_synchrony;         /**< Enable synchrony detection */

    /* Quantum acceleration */
    bool enable_quantum_features;   /**< Enable quantum feature map transformation */
    uint32_t quantum_output_dim;    /**< Quantum feature output dimension */
} feature_extractor_config_t;

/**
 * WHAT: Feature extractor engine instance
 * WHY:  Maintain state across multiple extractions
 * HOW:  Opaque handle pattern for encapsulation
 */
typedef struct feature_extractor_struct* feature_extractor_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * WHAT: Create feature extractor with configuration
 * WHY:  Initialize extraction engine and allocate resources
 * HOW:  Validate config, allocate buffers, initialize state
 *
 * @param config Extractor configuration (NULL uses defaults)
 * @return Extractor handle or NULL on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (each instance has independent state)
 */
feature_extractor_t feature_extractor_create(const feature_extractor_config_t* config);

/**
 * WHAT: Destroy feature extractor and free resources
 * WHY:  Clean memory cleanup
 * HOW:  Free all allocated memory, destroy mutex
 *
 * @param extractor Extractor to destroy (NULL is safe)
 *
 * COMPLEXITY: O(1)
 */
void feature_extractor_destroy(feature_extractor_t extractor);

/**
 * WHAT: Get default feature extractor configuration
 * WHY:  Provide sensible defaults for common use cases
 * HOW:  Return pre-filled config struct
 *
 * @return Default configuration
 *
 * DEFAULTS:
 * - window_ms: 100.0 (standard analysis window)
 * - synchrony_window_ms: 5.0 (biological coincidence window)
 * - burst_isi_threshold_ms: 10.0 (typical burst ISI)
 * - min_burst_spikes: 3 (minimum burst definition)
 * - entropy_bins: 20 (entropy histogram bins)
 * - compute_oscillations: true
 * - compute_entropy: true
 * - compute_synchrony: true
 */
feature_extractor_config_t feature_extractor_default_config(void);

//=============================================================================
// Main Extraction Function
//=============================================================================

/**
 * WHAT: Extract all features from spike data
 * WHY:  Single call to compute complete feature vector
 * HOW:  Dispatch to specialized feature computation functions
 *
 * ALGORITHM:
 * 1. Compute rate-based features (mean, std)
 * 2. Compute temporal features (ISI, CV, precision)
 * 3. Compute population features (synchrony, bursts, Fano)
 * 4. Compute oscillation power (delta-gamma bands)
 * 5. Compute information features (entropy)
 * 6. Assemble into unified feature vector
 *
 * @param extractor Extractor instance
 * @param spike_data Input spike data
 * @param features_out Output feature vector
 * @return true on success, false on error
 *
 * COMPLEXITY: O(n*s) where n=neurons, s=avg_spikes
 * THREAD-SAFE: Yes (uses internal mutex)
 */
bool feature_extractor_update(
    feature_extractor_t extractor,
    const spike_data_t* spike_data,
    middleware_features_t* features_out
);

//=============================================================================
// Individual Feature Computation
//=============================================================================

/**
 * WHAT: Compute mean firing rate across population
 * WHY:  Primary rate-based neural code
 * HOW:  Average spike count per neuron, normalize by window duration
 *
 * ALGORITHM:
 * rate = (total_spikes / num_neurons) / (window_duration_ms / 1000.0)
 *
 * @param extractor Extractor instance
 * @param spike_data Input spike data
 * @param rate_out Output mean firing rate (Hz)
 * @return true on success, false on error
 *
 * COMPLEXITY: O(n) where n = num_neurons
 */
bool feature_extractor_compute_mean_firing_rate(
    feature_extractor_t extractor,
    const spike_data_t* spike_data,
    float* rate_out
);

/**
 * WHAT: Compute coefficient of variation of ISI across population
 * WHY:  Measure spike train regularity and coding strategy
 * HOW:  Calculate CV = std(ISI) / mean(ISI) for each neuron, average
 *
 * INTERPRETATION:
 * - CV = 0: Perfectly regular (clock-like firing)
 * - CV = 1: Poisson process (random firing)
 * - CV > 1: Bursty, irregular firing
 *
 * @param extractor Extractor instance
 * @param spike_data Input spike data
 * @param cv_out Output population CV
 * @return true on success, false on error
 *
 * COMPLEXITY: O(n*s) where n=neurons, s=avg_spikes
 */
bool feature_extractor_compute_population_cv(
    feature_extractor_t extractor,
    const spike_data_t* spike_data,
    float* cv_out
);

/**
 * WHAT: Compute Fano factor (variance-to-mean ratio)
 * WHY:  Measure trial-to-trial variability and coding reliability
 * HOW:  Fano = variance(spike_counts) / mean(spike_counts)
 *
 * INTERPRETATION:
 * - Fano = 1: Poisson-like variability
 * - Fano < 1: Sub-Poisson (regular, reliable)
 * - Fano > 1: Super-Poisson (bursty, variable)
 *
 * @param extractor Extractor instance
 * @param spike_data Input spike data
 * @param fano_out Output Fano factor
 * @return true on success, false on error
 *
 * COMPLEXITY: O(n) where n = num_neurons
 */
bool feature_extractor_compute_fano_factor(
    feature_extractor_t extractor,
    const spike_data_t* spike_data,
    float* fano_out
);

/**
 * WHAT: Compute burst index (proportion of spikes in bursts)
 * WHY:  Bursts encode distinct information from tonic firing
 * HOW:  Detect bursts via ISI threshold, compute ratio
 *
 * BURST CRITERIA:
 * - ISI < burst_isi_threshold_ms for consecutive spikes
 * - Minimum min_burst_spikes spikes per burst
 *
 * @param extractor Extractor instance
 * @param spike_data Input spike data
 * @param burst_index_out Output burst index [0, 1]
 * @return true on success, false on error
 *
 * COMPLEXITY: O(n*s) where n=neurons, s=avg_spikes
 */
bool feature_extractor_compute_burst_index(
    feature_extractor_t extractor,
    const spike_data_t* spike_data,
    float* burst_index_out
);

/**
 * WHAT: Compute population synchrony index
 * WHY:  Synchrony indicates coordinated processing and binding
 * HOW:  Count spike coincidences, normalize by total spikes
 *
 * ALGORITHM:
 * 1. For each spike, count coincident spikes within window
 * 2. Coincidence = number of other neurons firing within ±sync_window_ms
 * 3. Synchrony = mean(coincidences) / num_neurons
 *
 * @param extractor Extractor instance
 * @param spike_data Input spike data
 * @param synchrony_out Output synchrony index [0, 1]
 * @return true on success, false on error
 *
 * COMPLEXITY: O(n*s^2) where n=neurons, s=avg_spikes
 * NOTE: Expensive for large populations, consider sampling
 */
bool feature_extractor_compute_synchrony_index(
    feature_extractor_t extractor,
    const spike_data_t* spike_data,
    float* synchrony_out
);

/**
 * WHAT: Compute oscillation power across frequency bands
 * WHY:  Oscillations reflect brain state and cognitive mode
 * HOW:  Construct rate signal, bin by frequency, compute power
 *
 * ALGORITHM:
 * 1. Create population rate time series (binned spike counts)
 * 2. For each frequency band:
 *    a. Count oscillation cycles in rate signal
 *    b. Compute power as variance in band-passed signal
 * 3. Normalize power by total power
 *
 * @param extractor Extractor instance
 * @param spike_data Input spike data
 * @param delta_power Output delta power (0.5-4 Hz)
 * @param theta_power Output theta power (4-8 Hz)
 * @param alpha_power Output alpha power (8-13 Hz)
 * @param beta_power Output beta power (13-30 Hz)
 * @param gamma_power Output gamma power (30-100 Hz)
 * @return true on success, false on error
 *
 * COMPLEXITY: O(n*s + b*w) where n=neurons, s=spikes, b=bins, w=window
 */
bool feature_extractor_compute_oscillation_power(
    feature_extractor_t extractor,
    const spike_data_t* spike_data,
    float* delta_power,
    float* theta_power,
    float* alpha_power,
    float* beta_power,
    float* gamma_power
);

/**
 * WHAT: Compute spike entropy (Shannon entropy)
 * WHY:  Measure information content and coding efficiency
 * HOW:  Histogram spike counts, calculate entropy
 *
 * ALGORITHM:
 * 1. Bin spike counts into histogram
 * 2. Normalize to probability distribution
 * 3. Entropy = -sum(p * log2(p)) for all bins
 *
 * @param extractor Extractor instance
 * @param spike_data Input spike data
 * @param entropy_out Output entropy (bits)
 * @return true on success, false on error
 *
 * COMPLEXITY: O(n) where n = num_neurons
 */
bool feature_extractor_compute_spike_entropy(
    feature_extractor_t extractor,
    const spike_data_t* spike_data,
    float* entropy_out
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * WHAT: Create middleware features structure
 * WHY:  Initialize feature vector
 * HOW:  Allocate and zero-initialize
 *
 * @return Allocated features or NULL on error
 */
middleware_features_t* middleware_features_create(void);

/**
 * WHAT: Destroy middleware features structure
 * WHY:  Free allocated memory
 * HOW:  Free structure
 *
 * @param features Features to destroy (NULL is safe)
 */
void middleware_features_destroy(middleware_features_t* features);

/**
 * WHAT: Create spike data structure
 * WHY:  Initialize spike data container
 * HOW:  Allocate arrays for spike times and counts
 *
 * @param num_neurons Number of neurons in population
 * @return Allocated spike data or NULL on error
 */
spike_data_t* spike_data_create(uint32_t num_neurons);

/**
 * WHAT: Destroy spike data structure
 * WHY:  Free all allocated memory
 * HOW:  Free spike arrays and structure
 *
 * @param data Spike data to destroy (NULL is safe)
 */
void spike_data_destroy(spike_data_t* data);

/**
 * WHAT: Reset features to invalid state
 * WHY:  Mark features as stale after context change
 * HOW:  Set valid flag to false
 *
 * @param features Features to reset
 */
void middleware_features_reset(middleware_features_t* features);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_FEATURE_EXTRACTOR_H
