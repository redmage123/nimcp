//=============================================================================
// nimcp_neural_statistics.h - Neural-Specific Statistics Module
//=============================================================================
/**
 * @file nimcp_neural_statistics.h
 * @brief Comprehensive neural-specific statistical analysis tools
 *
 * WHAT: Specialized statistics for neural data analysis including spike trains,
 *       population coding, cross-correlation, and synaptic transmission
 *
 * WHY:  Neural computation generates unique data patterns (spike trains, population
 *       activity, synaptic events) that require specialized statistical methods.
 *       Standard statistics are insufficient for capturing the temporal dynamics,
 *       information content, and correlational structure of neural signals.
 *
 * HOW:  C99 implementation with SIMD optimization, GPU acceleration for batch
 *       operations, and integration with NIMCP's immune and logging systems
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Neural Data Analysis Hierarchy:
 *   ┌─────────────────────────────────────────────────────────────────────────┐
 *   │  Single Neuron Level:                                                   │
 *   │    Point Process Statistics                                             │
 *   │    - Spike trains as stochastic point processes                         │
 *   │    - ISI distributions (exponential, gamma, inverse Gaussian)           │
 *   │    - Firing rate estimation (instantaneous, average)                    │
 *   │    - Burst detection and classification                                 │
 *   │                                                                         │
 *   │  Population Level:                                                      │
 *   │    Information Encoding Statistics                                      │
 *   │    - Fisher information: precision of neural codes                      │
 *   │    - Population vector decoding                                         │
 *   │    - Cramer-Rao bounds on decoding accuracy                             │
 *   │    - Tuning curve characterization                                      │
 *   │                                                                         │
 *   │  Connectivity Level:                                                    │
 *   │    Cross-Correlation Methods                                            │
 *   │    - Spike train cross-correlograms                                     │
 *   │    - Joint PSTH (JPSTH) for stimulus-locked analysis                    │
 *   │    - Spike-triggered averages                                           │
 *   │    - Spike-field coherence                                              │
 *   │                                                                         │
 *   │  Synaptic Level:                                                        │
 *   │    Transmission Statistics                                              │
 *   │    - Quantal analysis (n, p, q parameters)                              │
 *   │    - Release probability estimation                                     │
 *   │    - Paired-pulse ratio                                                 │
 *   │    - Short-term plasticity characterization                             │
 *   └─────────────────────────────────────────────────────────────────────────┘
 *
 * PERFORMANCE:
 * - ISI statistics (N=10000 spikes): <1ms
 * - Cross-correlogram (1000 bins): <5ms
 * - Fisher information (100 neurons): <10ms
 * - GPU batch processing: 10-100x speedup
 *
 * THREAD SAFETY:
 * - All functions are reentrant
 * - Uses thread-local RNG for sampling operations
 * - No global mutable state after initialization
 *
 * INTEGRATION:
 * - NIMCP_THROW_TO_IMMUNE for error reporting
 * - Health agent heartbeats for long operations
 * - nimcp_log_* for diagnostic output
 *
 * @author NIMCP Development Team
 * @date 2026-01-30
 * @version 1.0.0
 */

#ifndef NIMCP_NEURAL_STATISTICS_H
#define NIMCP_NEURAL_STATISTICS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Default maximum lag for cross-correlation (ms) */
#define NEURAL_STATS_DEFAULT_MAX_LAG_MS 100.0f

/** Default bin width for correlograms (ms) */
#define NEURAL_STATS_DEFAULT_BIN_WIDTH_MS 1.0f

/** Default number of bins for histograms */
#define NEURAL_STATS_DEFAULT_BINS 50

/** Maximum neurons for Fisher information matrix */
#define NEURAL_STATS_MAX_FISHER_NEURONS 1000

/** Tolerance for numerical comparisons */
#define NEURAL_STATS_EPSILON 1e-10f

/** Maximum iterations for fitting algorithms */
#define NEURAL_STATS_MAX_ITERATIONS 1000

/** Default burst threshold (ISI ratio) */
#define NEURAL_STATS_DEFAULT_BURST_THRESHOLD 0.5f

/** Health agent heartbeat interval (operations) */
#define NEURAL_STATS_HEARTBEAT_INTERVAL 10000

//=============================================================================
// Error Codes
//=============================================================================

/**
 * @brief Result codes for neural statistics operations
 */
typedef enum neural_stats_result {
    NEURAL_STATS_OK = 0,                /**< Success */
    NEURAL_STATS_ERROR_NULL = -1,       /**< NULL pointer argument */
    NEURAL_STATS_ERROR_SIZE = -2,       /**< Invalid size (n=0 or too small) */
    NEURAL_STATS_ERROR_MEMORY = -3,     /**< Memory allocation failed */
    NEURAL_STATS_ERROR_PARAMS = -4,     /**< Invalid parameters */
    NEURAL_STATS_ERROR_CONVERGE = -5,   /**< Algorithm did not converge */
    NEURAL_STATS_ERROR_SINGULAR = -6,   /**< Singular matrix encountered */
    NEURAL_STATS_ERROR_RANGE = -7,      /**< Value out of valid range */
    NEURAL_STATS_ERROR_NOT_INIT = -8,   /**< Module not initialized */
    NEURAL_STATS_ERROR_GPU = -9         /**< GPU operation failed */
} neural_stats_result_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Configuration for neural statistics module
 */
typedef struct neural_stats_config {
    bool enable_simd;            /**< Use SIMD vectorization if available */
    bool enable_gpu;             /**< Use GPU acceleration if available */
    bool enable_parallel;        /**< Use parallel computation for large arrays */
    uint32_t parallel_threshold; /**< Array size threshold for parallelization */
    float default_bin_width_ms;  /**< Default bin width for histograms (ms) */
    uint32_t random_seed;        /**< Seed for reproducible results (0 = random) */
} neural_stats_config_t;

//=============================================================================
// Spike Train Structures
//=============================================================================

/**
 * @brief Spike train representation
 *
 * Represents a sequence of spike times from a single neuron.
 * Times should be sorted in ascending order.
 */
typedef struct neural_spike_train {
    float* spike_times;     /**< Spike times in milliseconds */
    uint32_t n_spikes;      /**< Number of spikes */
    float start_time;       /**< Recording start time (ms) */
    float end_time;         /**< Recording end time (ms) */
    uint32_t neuron_id;     /**< Neuron identifier */
} neural_spike_train_t;

/**
 * @brief Spike train ensemble (multiple neurons)
 */
typedef struct neural_spike_ensemble {
    neural_spike_train_t* trains;  /**< Array of spike trains */
    uint32_t n_neurons;            /**< Number of neurons */
    float start_time;              /**< Common start time (ms) */
    float end_time;                /**< Common end time (ms) */
} neural_spike_ensemble_t;

//=============================================================================
// ISI Statistics Structures
//=============================================================================

/**
 * @brief Inter-spike interval distribution result
 */
typedef struct neural_isi_distribution {
    float* intervals;        /**< ISI values (caller may keep or free) */
    uint32_t n_intervals;    /**< Number of intervals (n_spikes - 1) */
    float mean;              /**< Mean ISI (ms) */
    float variance;          /**< Variance of ISI (ms^2) */
    float std_dev;           /**< Standard deviation (ms) */
    float cv;                /**< Coefficient of variation (std/mean) */
    float cv2;               /**< Local CV (CV of consecutive pairs) */
    float min_isi;           /**< Minimum ISI (ms) */
    float max_isi;           /**< Maximum ISI (ms) */
    float median_isi;        /**< Median ISI (ms) */
    float skewness;          /**< Skewness of distribution */
    float kurtosis;          /**< Excess kurtosis */
} neural_isi_distribution_t;

/**
 * @brief Firing rate estimation result
 */
typedef struct neural_firing_rate {
    float mean_rate;         /**< Mean firing rate (Hz) */
    float* instantaneous;    /**< Instantaneous rate array (if computed) */
    float* rate_times;       /**< Time points for instantaneous rates */
    uint32_t n_time_points;  /**< Number of time points */
    float peak_rate;         /**< Peak instantaneous rate (Hz) */
    float rate_variance;     /**< Variance of rate over time */
} neural_firing_rate_t;

/**
 * @brief Burst detection result
 */
typedef struct neural_burst_result {
    uint32_t n_bursts;       /**< Number of bursts detected */
    float* burst_starts;     /**< Start times of bursts (ms) */
    float* burst_ends;       /**< End times of bursts (ms) */
    uint32_t* spikes_per_burst; /**< Number of spikes in each burst */
    float mean_burst_duration;  /**< Mean burst duration (ms) */
    float mean_spikes_per_burst; /**< Mean spikes per burst */
    float burst_rate;        /**< Bursts per second */
    float intra_burst_freq;  /**< Mean intra-burst frequency (Hz) */
    float fraction_in_bursts; /**< Fraction of spikes in bursts */
} neural_burst_result_t;

/**
 * @brief Fano factor result
 */
typedef struct neural_fano_result {
    float fano_factor;       /**< Variance/mean of spike counts */
    float* fano_by_window;   /**< Fano factor for different window sizes */
    float* window_sizes;     /**< Window sizes used (ms) */
    uint32_t n_windows;      /**< Number of window sizes tested */
    float expected_poisson;  /**< Expected FF for Poisson (1.0) */
    bool is_sub_poisson;     /**< True if FF < 1 (regular) */
    bool is_super_poisson;   /**< True if FF > 1 (bursty) */
} neural_fano_result_t;

/**
 * @brief Spike train entropy result
 */
typedef struct neural_entropy_result {
    float total_entropy;     /**< Total spike train entropy (bits) */
    float entropy_rate;      /**< Entropy rate (bits/spike) */
    float mutual_info;       /**< Mutual information with stimulus (if provided) */
    float noise_entropy;     /**< Noise entropy (bits) */
    float signal_entropy;    /**< Signal entropy (bits) */
    uint32_t n_patterns;     /**< Number of unique patterns observed */
} neural_entropy_result_t;

//=============================================================================
// Population Coding Structures
//=============================================================================

/**
 * @brief Tuning curve types
 */
typedef enum neural_tuning_type {
    NEURAL_TUNING_GAUSSIAN,      /**< Gaussian tuning: f(s) = A*exp(-(s-s0)^2/(2*sigma^2)) */
    NEURAL_TUNING_VON_MISES,     /**< Von Mises (circular): f(s) = A*exp(kappa*cos(s-s0)) */
    NEURAL_TUNING_COSINE,        /**< Cosine tuning: f(s) = A*cos(s-s0) + B */
    NEURAL_TUNING_SIGMOID,       /**< Sigmoid tuning: f(s) = A/(1+exp(-(s-s0)/sigma)) */
    NEURAL_TUNING_MONOTONIC,     /**< Monotonic (power law): f(s) = A*s^n */
    NEURAL_TUNING_POISSON        /**< Poisson GLM tuning */
} neural_tuning_type_t;

/**
 * @brief Tuning curve parameters
 */
typedef struct neural_tuning_params {
    neural_tuning_type_t type;   /**< Type of tuning curve */
    float amplitude;             /**< Peak amplitude (A) */
    float baseline;              /**< Baseline firing rate (B) */
    float preferred;             /**< Preferred stimulus (s0) */
    float width;                 /**< Tuning width (sigma or kappa) */
    float exponent;              /**< Exponent for power law (n) */
    float r_squared;             /**< Goodness of fit */
    float aic;                   /**< Akaike Information Criterion */
} neural_tuning_params_t;

/**
 * @brief Fisher information result
 */
typedef struct neural_fisher_info {
    float fisher_info;           /**< Fisher information (scalar for 1D) */
    float* fisher_matrix;        /**< Fisher information matrix [n_params x n_params] */
    uint32_t n_params;           /**< Number of parameters */
    float* cramer_rao_bounds;    /**< Cramer-Rao lower bounds on variance */
    float total_info;            /**< Sum of diagonal elements */
} neural_fisher_info_t;

/**
 * @brief Population vector decoding result
 */
typedef struct neural_population_vector {
    float* decoded_stimulus;     /**< Decoded stimulus values */
    uint32_t n_time_points;      /**< Number of decoded time points */
    float* confidence;           /**< Confidence/reliability at each point */
    float mean_error;            /**< Mean decoding error (if true stimulus known) */
    float rmse;                  /**< Root mean squared error */
} neural_population_vector_t;

/**
 * @brief Decoding error bound result
 */
typedef struct neural_decoding_bound {
    float cramer_rao_bound;      /**< Cramer-Rao lower bound on MSE */
    float achieved_error;        /**< Achieved MSE (if decoder provided) */
    float efficiency;            /**< Achieved/bound ratio (>1 means suboptimal) */
    bool is_efficient;           /**< True if within 10% of bound */
} neural_decoding_bound_t;

//=============================================================================
// Cross-Correlation Structures
//=============================================================================

/**
 * @brief Cross-correlogram result
 */
typedef struct neural_cross_correlogram {
    float* correlogram;          /**< Correlation values at each lag */
    float* lags;                 /**< Lag values (ms) */
    uint32_t n_bins;             /**< Number of bins */
    float bin_width;             /**< Bin width (ms) */
    float max_lag;               /**< Maximum lag (ms) */
    float peak_correlation;      /**< Peak correlation value */
    float peak_lag;              /**< Lag at peak */
    float shuffle_mean;          /**< Mean from shuffle predictor */
    float shuffle_std;           /**< Std from shuffle predictor */
    float significance_threshold; /**< Threshold for significance */
    bool* is_significant;        /**< Whether each bin is significant */
} neural_cross_correlogram_t;

/**
 * @brief Joint PSTH result
 */
typedef struct neural_jpsth {
    float* jpsth_matrix;         /**< JPSTH matrix [n_bins x n_bins] */
    float* psth1;                /**< PSTH of neuron 1 */
    float* psth2;                /**< PSTH of neuron 2 */
    float* coincidence_histogram; /**< Diagonal sum (coincidence) */
    float* time_bins;            /**< Time bin centers (ms) */
    uint32_t n_bins;             /**< Number of bins per dimension */
    float bin_width;             /**< Bin width (ms) */
    float* normalized_jpsth;     /**< JPSTH normalized by shuffle predictor */
    float correlation_strength;  /**< Overall correlation strength */
} neural_jpsth_t;

/**
 * @brief Spike-triggered average result
 */
typedef struct neural_sta {
    float* sta;                  /**< Spike-triggered average waveform */
    float* sta_times;            /**< Time points relative to spike */
    uint32_t n_samples;          /**< Number of samples in STA */
    float* sta_std;              /**< Standard deviation at each point */
    float* confidence_upper;     /**< Upper confidence bound */
    float* confidence_lower;     /**< Lower confidence bound */
    uint32_t n_spikes_used;      /**< Number of spikes contributing */
    float peak_amplitude;        /**< Peak STA amplitude */
    float peak_time;             /**< Time of peak (ms before spike) */
} neural_sta_t;

/**
 * @brief Spike-field coherence result
 */
typedef struct neural_spike_field_coherence {
    float* coherence;            /**< Coherence at each frequency */
    float* frequencies;          /**< Frequency values (Hz) */
    uint32_t n_frequencies;      /**< Number of frequencies */
    float* phase;                /**< Phase at each frequency (radians) */
    float* phase_std;            /**< Phase standard deviation */
    float peak_coherence;        /**< Maximum coherence */
    float peak_frequency;        /**< Frequency of peak coherence */
    float* confidence_level;     /**< Confidence level for each frequency */
} neural_spike_field_coherence_t;

//=============================================================================
// Synaptic Statistics Structures
//=============================================================================

/**
 * @brief Quantal analysis result
 */
typedef struct neural_quantal_analysis {
    float n;                     /**< Number of release sites */
    float p;                     /**< Release probability */
    float q;                     /**< Quantal size (mV or pA) */
    float n_variance;            /**< Variance of n estimate */
    float p_variance;            /**< Variance of p estimate */
    float q_variance;            /**< Variance of q estimate */
    float* amplitude_histogram;  /**< EPSP/EPSC amplitude histogram */
    float* histogram_bins;       /**< Histogram bin centers */
    uint32_t n_bins;             /**< Number of histogram bins */
    float chi_squared;           /**< Chi-squared goodness of fit */
    float p_value;               /**< P-value for fit */
    bool is_binomial;            /**< True if binomial model fits */
} neural_quantal_analysis_t;

/**
 * @brief Release probability result
 */
typedef struct neural_release_prob {
    float p_release;             /**< Estimated release probability */
    float p_variance;            /**< Variance of estimate */
    float* p_by_stimulus;        /**< Release prob for each stimulus in train */
    uint32_t n_stimuli;          /**< Number of stimuli */
    float cv_amplitude;          /**< CV of response amplitudes */
    float failure_rate;          /**< Fraction of failures */
} neural_release_prob_t;

/**
 * @brief Paired-pulse ratio result
 */
typedef struct neural_ppr_result {
    float ppr;                   /**< Paired-pulse ratio (P2/P1) */
    float ppr_std;               /**< Standard deviation */
    float* ppr_by_interval;      /**< PPR for different intervals */
    float* intervals;            /**< Inter-stimulus intervals (ms) */
    uint32_t n_intervals;        /**< Number of intervals tested */
    bool is_facilitating;        /**< True if PPR > 1 */
    bool is_depressing;          /**< True if PPR < 1 */
    float facilitation_index;    /**< (P2-P1)/P1 */
    float recovery_time;         /**< Time constant for recovery (ms) */
} neural_ppr_result_t;

//=============================================================================
// Module Initialization
//=============================================================================

/**
 * @brief Get default configuration
 * @return Default neural statistics configuration
 */
NIMCP_EXPORT neural_stats_config_t neural_stats_default_config(void);

/**
 * @brief Initialize the neural statistics module
 * @param config Configuration (NULL for defaults)
 * @return NEURAL_STATS_OK on success
 *
 * WHAT: Initialize neural statistics subsystem
 * WHY:  Sets up SIMD/GPU detection, RNG integration
 * HOW:  One-time setup, thread-safe initialization
 */
NIMCP_EXPORT neural_stats_result_t neural_stats_init(const neural_stats_config_t* config);

/**
 * @brief Shutdown the neural statistics module
 *
 * WHAT: Clean up neural statistics subsystem
 * WHY:  Release cached resources
 * HOW:  Free internal allocations
 */
NIMCP_EXPORT void neural_stats_shutdown(void);

/**
 * @brief Check if module is initialized
 * @return true if initialized
 */
NIMCP_EXPORT bool neural_stats_is_initialized(void);

//=============================================================================
// Point Process Analysis - ISI Statistics
//=============================================================================

/**
 * @brief Compute inter-spike interval distribution
 *
 * WHAT: Analyze the distribution of times between consecutive spikes
 * WHY:  ISI distributions reveal firing regularity, refractoriness, and bursting
 * HOW:  Extract intervals, compute moments, fit parametric distributions
 *
 * NEUROSCIENCE:
 * - CV < 1: Regular firing (Poisson-like or more regular)
 * - CV = 1: Poisson process (exponential ISI)
 * - CV > 1: Bursty/irregular firing
 *
 * @param spike_train Input spike train
 * @param result Output ISI distribution result
 * @return NEURAL_STATS_OK on success
 *
 * Time complexity: O(n_spikes)
 */
NIMCP_EXPORT neural_stats_result_t nimcp_neural_isi_distribution(
    const neural_spike_train_t* spike_train,
    neural_isi_distribution_t* result
);

/**
 * @brief Compute coefficient of variation of ISI
 *
 * CV = std(ISI) / mean(ISI)
 *
 * @param spike_train Input spike train
 * @return CV value, or NAN on error
 */
NIMCP_EXPORT float nimcp_neural_isi_cv(const neural_spike_train_t* spike_train);

/**
 * @brief Compute local coefficient of variation (CV2)
 *
 * CV2 = 2|ISI_i - ISI_{i+1}| / (ISI_i + ISI_{i+1})
 * More robust to rate changes than global CV.
 *
 * @param spike_train Input spike train
 * @return Local CV value, or NAN on error
 */
NIMCP_EXPORT float nimcp_neural_isi_cv2(const neural_spike_train_t* spike_train);

/**
 * @brief Compute Fano factor for spike train
 *
 * WHAT: Compute variance/mean of spike counts in windows
 * WHY:  Fano factor indicates deviation from Poisson process
 * HOW:  Count spikes in sliding windows, compute ratio
 *
 * @param spike_train Input spike train
 * @param window_size Window size in ms
 * @param result Output Fano factor result
 * @return NEURAL_STATS_OK on success
 */
NIMCP_EXPORT neural_stats_result_t nimcp_neural_fano_factor(
    const neural_spike_train_t* spike_train,
    float window_size,
    neural_fano_result_t* result
);

/**
 * @brief Compute entropy of spike train
 *
 * WHAT: Information-theoretic analysis of spike patterns
 * WHY:  Quantify information capacity and coding efficiency
 * HOW:  Discretize spike train into patterns, compute Shannon entropy
 *
 * @param spike_train Input spike train
 * @param bin_size Bin size for discretization (ms)
 * @param word_length Number of bins per pattern
 * @param result Output entropy result
 * @return NEURAL_STATS_OK on success
 */
NIMCP_EXPORT neural_stats_result_t nimcp_neural_spike_train_entropy(
    const neural_spike_train_t* spike_train,
    float bin_size,
    uint32_t word_length,
    neural_entropy_result_t* result
);

/**
 * @brief Compute firing rate (instantaneous and average)
 *
 * WHAT: Estimate firing rate over time
 * WHY:  Rate coding is fundamental to neural information processing
 * HOW:  Kernel density estimation or histogram methods
 *
 * @param spike_train Input spike train
 * @param kernel_width Width of Gaussian kernel (ms), 0 for histogram
 * @param time_step Time step for rate estimation (ms)
 * @param result Output firing rate result
 * @return NEURAL_STATS_OK on success
 */
NIMCP_EXPORT neural_stats_result_t nimcp_neural_firing_rate(
    const neural_spike_train_t* spike_train,
    float kernel_width,
    float time_step,
    neural_firing_rate_t* result
);

/**
 * @brief Detect bursting patterns in spike train
 *
 * WHAT: Identify bursts of high-frequency spikes
 * WHY:  Bursts carry special information and indicate specific neural states
 * HOW:  ISI-based detection with configurable thresholds
 *
 * @param spike_train Input spike train
 * @param max_isi_within Maximum ISI within burst (ms)
 * @param min_spikes_per_burst Minimum spikes to constitute a burst
 * @param result Output burst detection result
 * @return NEURAL_STATS_OK on success
 */
NIMCP_EXPORT neural_stats_result_t nimcp_neural_burst_detection(
    const neural_spike_train_t* spike_train,
    float max_isi_within,
    uint32_t min_spikes_per_burst,
    neural_burst_result_t* result
);

//=============================================================================
// Point Process Analysis - Memory Functions
//=============================================================================

/**
 * @brief Free ISI distribution result
 */
NIMCP_EXPORT void nimcp_neural_isi_distribution_free(neural_isi_distribution_t* result);

/**
 * @brief Free firing rate result
 */
NIMCP_EXPORT void nimcp_neural_firing_rate_free(neural_firing_rate_t* result);

/**
 * @brief Free burst result
 */
NIMCP_EXPORT void nimcp_neural_burst_result_free(neural_burst_result_t* result);

/**
 * @brief Free Fano result
 */
NIMCP_EXPORT void nimcp_neural_fano_result_free(neural_fano_result_t* result);

/**
 * @brief Free entropy result
 */
NIMCP_EXPORT void nimcp_neural_entropy_result_free(neural_entropy_result_t* result);

//=============================================================================
// Population Coding Statistics
//=============================================================================

/**
 * @brief Compute Fisher information for neural population
 *
 * WHAT: Quantify precision of neural population code
 * WHY:  Fisher information bounds decoding accuracy via Cramer-Rao
 * HOW:  Compute FI from tuning curves and noise correlations
 *
 * MATHEMATICAL FOUNDATION:
 * For Poisson neurons with tuning curves f_i(s):
 *   I(s) = sum_i (df_i/ds)^2 / f_i(s)
 *
 * With noise correlations (covariance C):
 *   I(s) = f'^T C^{-1} f'
 *
 * @param tuning_curves Tuning curve values [n_neurons x n_stimuli]
 * @param stimulus_values Stimulus values [n_stimuli]
 * @param n_neurons Number of neurons
 * @param n_stimuli Number of stimulus values
 * @param noise_cov Noise covariance matrix [n_neurons x n_neurons], NULL for independent
 * @param result Output Fisher information result
 * @return NEURAL_STATS_OK on success
 */
NIMCP_EXPORT neural_stats_result_t nimcp_neural_fisher_information(
    const float* tuning_curves,
    const float* stimulus_values,
    uint32_t n_neurons,
    uint32_t n_stimuli,
    const float* noise_cov,
    neural_fisher_info_t* result
);

/**
 * @brief Population vector decoding
 *
 * WHAT: Decode stimulus from population activity using vector sum
 * WHY:  Simple, biologically plausible decoding mechanism
 * HOW:  Weight preferred directions by firing rates
 *
 * @param spike_counts Spike counts [n_neurons x n_time_points]
 * @param preferred_stimuli Preferred stimulus for each neuron [n_neurons]
 * @param n_neurons Number of neurons
 * @param n_time_points Number of time points
 * @param result Output decoded stimulus values
 * @return NEURAL_STATS_OK on success
 */
NIMCP_EXPORT neural_stats_result_t nimcp_neural_population_vector(
    const float* spike_counts,
    const float* preferred_stimuli,
    uint32_t n_neurons,
    uint32_t n_time_points,
    neural_population_vector_t* result
);

/**
 * @brief Compute Cramer-Rao bound for decoding error
 *
 * WHAT: Lower bound on achievable decoding error
 * WHY:  Benchmark for evaluating decoder optimality
 * HOW:  Invert Fisher information matrix
 *
 * @param fisher_info Fisher information matrix
 * @param n_params Number of parameters
 * @param result Output decoding bound result
 * @return NEURAL_STATS_OK on success
 */
NIMCP_EXPORT neural_stats_result_t nimcp_neural_decoding_error_bound(
    const float* fisher_info,
    uint32_t n_params,
    neural_decoding_bound_t* result
);

/**
 * @brief Fit tuning curve to neural data
 *
 * WHAT: Estimate tuning curve parameters from spike data
 * WHY:  Characterize neuronal selectivity
 * HOW:  Nonlinear least squares or maximum likelihood
 *
 * @param stimulus_values Stimulus values [n_trials]
 * @param firing_rates Observed firing rates [n_trials]
 * @param n_trials Number of trials
 * @param tuning_type Type of tuning curve to fit
 * @param result Output tuning curve parameters
 * @return NEURAL_STATS_OK on success
 */
NIMCP_EXPORT neural_stats_result_t nimcp_neural_tuning_curve_fit(
    const float* stimulus_values,
    const float* firing_rates,
    uint32_t n_trials,
    neural_tuning_type_t tuning_type,
    neural_tuning_params_t* result
);

//=============================================================================
// Population Coding - Memory Functions
//=============================================================================

/**
 * @brief Free Fisher information result
 */
NIMCP_EXPORT void nimcp_neural_fisher_info_free(neural_fisher_info_t* result);

/**
 * @brief Free population vector result
 */
NIMCP_EXPORT void nimcp_neural_population_vector_free(neural_population_vector_t* result);

//=============================================================================
// Cross-Correlation Methods
//=============================================================================

/**
 * @brief Compute cross-correlogram between two spike trains
 *
 * WHAT: Temporal correlation structure between spike trains
 * WHY:  Reveals functional connectivity and synchrony
 * HOW:  Histogram of spike time differences
 *
 * NEUROSCIENCE:
 * - Sharp peak at lag 0: Synchronous firing (common input or connection)
 * - Asymmetric peak: Directional influence (earlier train -> later)
 * - Oscillatory structure: Rhythmic coupling
 *
 * @param train1 First spike train (reference)
 * @param train2 Second spike train (target)
 * @param bin_width Bin width in ms
 * @param max_lag Maximum lag to compute (ms)
 * @param result Output cross-correlogram
 * @return NEURAL_STATS_OK on success
 */
NIMCP_EXPORT neural_stats_result_t nimcp_neural_cross_correlogram(
    const neural_spike_train_t* train1,
    const neural_spike_train_t* train2,
    float bin_width,
    float max_lag,
    neural_cross_correlogram_t* result
);

/**
 * @brief Compute joint peristimulus time histogram (JPSTH)
 *
 * WHAT: 2D histogram of spike times relative to stimulus
 * WHY:  Stimulus-locked correlation analysis
 * HOW:  Histogram of (t1, t2) pairs for each trial
 *
 * @param train1 First spike train
 * @param train2 Second spike train
 * @param event_times Stimulus/event times [n_events]
 * @param n_events Number of events
 * @param window_before Time before event (ms)
 * @param window_after Time after event (ms)
 * @param bin_width Bin width (ms)
 * @param result Output JPSTH result
 * @return NEURAL_STATS_OK on success
 */
NIMCP_EXPORT neural_stats_result_t nimcp_neural_jpsth(
    const neural_spike_train_t* train1,
    const neural_spike_train_t* train2,
    const float* event_times,
    uint32_t n_events,
    float window_before,
    float window_after,
    float bin_width,
    neural_jpsth_t* result
);

/**
 * @brief Compute spike-triggered average
 *
 * WHAT: Average stimulus/signal preceding spikes
 * WHY:  Reveals receptive field / input selectivity
 * HOW:  Average signal windows aligned to spike times
 *
 * @param spike_train Spike train
 * @param signal Continuous signal [n_samples]
 * @param signal_times Time stamps of signal samples
 * @param n_samples Number of signal samples
 * @param window_before Time before spike (ms)
 * @param window_after Time after spike (ms)
 * @param result Output STA result
 * @return NEURAL_STATS_OK on success
 */
NIMCP_EXPORT neural_stats_result_t nimcp_neural_spike_triggered_average(
    const neural_spike_train_t* spike_train,
    const float* signal,
    const float* signal_times,
    uint32_t n_samples,
    float window_before,
    float window_after,
    neural_sta_t* result
);

/**
 * @brief Compute spike-field coherence
 *
 * WHAT: Frequency-domain correlation between spikes and LFP
 * WHY:  Reveals phase-locking of spikes to oscillatory activity
 * HOW:  Cross-spectral analysis of point process and signal
 *
 * @param spike_train Spike train
 * @param lfp Local field potential signal [n_samples]
 * @param lfp_times LFP time stamps [n_samples]
 * @param n_samples Number of LFP samples
 * @param freq_min Minimum frequency (Hz)
 * @param freq_max Maximum frequency (Hz)
 * @param n_frequencies Number of frequency bins
 * @param result Output coherence result
 * @return NEURAL_STATS_OK on success
 */
NIMCP_EXPORT neural_stats_result_t nimcp_neural_spike_field_coherence(
    const neural_spike_train_t* spike_train,
    const float* lfp,
    const float* lfp_times,
    uint32_t n_samples,
    float freq_min,
    float freq_max,
    uint32_t n_frequencies,
    neural_spike_field_coherence_t* result
);

//=============================================================================
// Cross-Correlation - Memory Functions
//=============================================================================

/**
 * @brief Free cross-correlogram result
 */
NIMCP_EXPORT void nimcp_neural_cross_correlogram_free(neural_cross_correlogram_t* result);

/**
 * @brief Free JPSTH result
 */
NIMCP_EXPORT void nimcp_neural_jpsth_free(neural_jpsth_t* result);

/**
 * @brief Free STA result
 */
NIMCP_EXPORT void nimcp_neural_sta_free(neural_sta_t* result);

/**
 * @brief Free spike-field coherence result
 */
NIMCP_EXPORT void nimcp_neural_spike_field_coherence_free(neural_spike_field_coherence_t* result);

//=============================================================================
// Synaptic Statistics
//=============================================================================

/**
 * @brief Perform quantal analysis on synaptic responses
 *
 * WHAT: Estimate n, p, q parameters of synaptic transmission
 * WHY:  Characterize synaptic properties and plasticity mechanisms
 * HOW:  Fit binomial model to amplitude histograms
 *
 * PARAMETERS:
 * - n: Number of release sites
 * - p: Release probability per site
 * - q: Quantal size (single vesicle response)
 *
 * @param amplitudes Response amplitudes [n_trials]
 * @param n_trials Number of trials
 * @param baseline_noise Baseline noise level (for failure detection)
 * @param result Output quantal analysis result
 * @return NEURAL_STATS_OK on success
 */
NIMCP_EXPORT neural_stats_result_t nimcp_neural_quantal_analysis(
    const float* amplitudes,
    uint32_t n_trials,
    float baseline_noise,
    neural_quantal_analysis_t* result
);

/**
 * @brief Estimate synaptic release probability
 *
 * WHAT: Estimate probability of vesicle release
 * WHY:  Key parameter for synaptic plasticity models
 * HOW:  Variance-mean analysis or failure rate
 *
 * @param amplitudes Response amplitudes [n_trials]
 * @param n_trials Number of trials
 * @param quantal_size Known or estimated quantal size
 * @param result Output release probability result
 * @return NEURAL_STATS_OK on success
 */
NIMCP_EXPORT neural_stats_result_t nimcp_neural_release_probability(
    const float* amplitudes,
    uint32_t n_trials,
    float quantal_size,
    neural_release_prob_t* result
);

/**
 * @brief Compute paired-pulse ratio
 *
 * WHAT: Ratio of second to first response in paired stimuli
 * WHY:  Indicates presynaptic short-term plasticity
 * HOW:  Average P2/P1 across trials
 *
 * INTERPRETATION:
 * - PPR > 1: Facilitation (low initial p, increased by residual Ca2+)
 * - PPR < 1: Depression (high initial p, vesicle depletion)
 *
 * @param amplitude1 First pulse amplitudes [n_trials]
 * @param amplitude2 Second pulse amplitudes [n_trials]
 * @param n_trials Number of paired-pulse trials
 * @param inter_stimulus_interval Interval between pulses (ms)
 * @param result Output PPR result
 * @return NEURAL_STATS_OK on success
 */
NIMCP_EXPORT neural_stats_result_t nimcp_neural_paired_pulse_ratio(
    const float* amplitude1,
    const float* amplitude2,
    uint32_t n_trials,
    float inter_stimulus_interval,
    neural_ppr_result_t* result
);

//=============================================================================
// Synaptic Statistics - Memory Functions
//=============================================================================

/**
 * @brief Free quantal analysis result
 */
NIMCP_EXPORT void nimcp_neural_quantal_analysis_free(neural_quantal_analysis_t* result);

/**
 * @brief Free release probability result
 */
NIMCP_EXPORT void nimcp_neural_release_prob_free(neural_release_prob_t* result);

/**
 * @brief Free PPR result
 */
NIMCP_EXPORT void nimcp_neural_ppr_result_free(neural_ppr_result_t* result);

//=============================================================================
// Batch GPU Operations
//=============================================================================

/**
 * @brief Batch ISI computation on GPU
 *
 * @param spike_trains Array of spike trains [n_trains]
 * @param n_trains Number of spike trains
 * @param results Output array of ISI distributions [n_trains]
 * @return NEURAL_STATS_OK on success
 */
NIMCP_EXPORT neural_stats_result_t nimcp_neural_isi_distribution_batch_gpu(
    const neural_spike_train_t* spike_trains,
    uint32_t n_trains,
    neural_isi_distribution_t* results
);

/**
 * @brief Batch cross-correlogram computation on GPU
 *
 * @param trains1 First spike trains [n_pairs]
 * @param trains2 Second spike trains [n_pairs]
 * @param n_pairs Number of pairs
 * @param bin_width Bin width (ms)
 * @param max_lag Maximum lag (ms)
 * @param results Output correlograms [n_pairs]
 * @return NEURAL_STATS_OK on success
 */
NIMCP_EXPORT neural_stats_result_t nimcp_neural_cross_correlogram_batch_gpu(
    const neural_spike_train_t* trains1,
    const neural_spike_train_t* trains2,
    uint32_t n_pairs,
    float bin_width,
    float max_lag,
    neural_cross_correlogram_t* results
);

/**
 * @brief Batch Fisher information on GPU
 *
 * @param tuning_curves Tuning curves [n_populations x n_neurons x n_stimuli]
 * @param stimulus_values Stimulus values [n_stimuli]
 * @param n_populations Number of populations
 * @param n_neurons Neurons per population
 * @param n_stimuli Number of stimulus values
 * @param results Output Fisher info [n_populations]
 * @return NEURAL_STATS_OK on success
 */
NIMCP_EXPORT neural_stats_result_t nimcp_neural_fisher_information_batch_gpu(
    const float* tuning_curves,
    const float* stimulus_values,
    uint32_t n_populations,
    uint32_t n_neurons,
    uint32_t n_stimuli,
    neural_fisher_info_t* results
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Create spike train from spike times array
 *
 * @param spike_times Spike times in ms
 * @param n_spikes Number of spikes
 * @param start_time Recording start time
 * @param end_time Recording end time
 * @param neuron_id Neuron identifier
 * @return Allocated spike train (caller must free)
 */
NIMCP_EXPORT neural_spike_train_t* nimcp_neural_spike_train_create(
    const float* spike_times,
    uint32_t n_spikes,
    float start_time,
    float end_time,
    uint32_t neuron_id
);

/**
 * @brief Destroy spike train
 */
NIMCP_EXPORT void nimcp_neural_spike_train_destroy(neural_spike_train_t* train);

/**
 * @brief Create spike ensemble from array of trains
 *
 * @param trains Array of spike trains
 * @param n_neurons Number of neurons
 * @return Allocated ensemble (caller must free)
 */
NIMCP_EXPORT neural_spike_ensemble_t* nimcp_neural_spike_ensemble_create(
    const neural_spike_train_t* trains,
    uint32_t n_neurons
);

/**
 * @brief Destroy spike ensemble
 */
NIMCP_EXPORT void nimcp_neural_spike_ensemble_destroy(neural_spike_ensemble_t* ensemble);

/**
 * @brief Get error message for result code
 * @param result Result code
 * @return Human-readable error message
 */
NIMCP_EXPORT const char* nimcp_neural_stats_error_string(neural_stats_result_t result);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_NEURAL_STATISTICS_H
