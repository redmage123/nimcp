/**
 * @file nimcp_fractal.h
 * @brief Fractal Analysis Module for Prime Resonant System
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Analyze fractal/self-similar properties of signals for memory dynamics
 * WHY:  Validate pink noise characteristics, measure long-range correlations,
 *       and detect scale-invariant patterns in neural signals and memory traces
 * HOW:  Implements Hurst exponent (R/S analysis), Detrended Fluctuation Analysis,
 *       spectral exponent estimation, box-counting dimension, lacunarity, and
 *       multifractal spectrum analysis
 *
 * NEUROSCIENCE APPLICATIONS:
 * ==========================
 * - Pink noise validation: Verify 1/f characteristics (alpha ~ 1.0)
 * - Memory dynamics: Long-range temporal correlations in recall patterns
 * - Neural criticality: Self-organized criticality detection
 * - Sleep stages: Fractal dimension changes during sleep
 * - Cognitive states: Complexity changes during task performance
 *
 * MATHEMATICAL BACKGROUND:
 * ========================
 * 1. HURST EXPONENT (H):
 *    - H = 0.5: Random walk (uncorrelated, Brownian motion)
 *    - H > 0.5: Persistent (trending, positive correlation)
 *    - H < 0.5: Anti-persistent (mean-reverting, negative correlation)
 *    - For 1/f noise: H = (alpha + 1) / 2
 *
 * 2. DFA EXPONENT (alpha):
 *    - alpha = 0.5: White noise (uncorrelated)
 *    - alpha = 1.0: Pink noise (1/f)
 *    - alpha = 1.5: Brown noise (1/f^2)
 *    - Related to Hurst: H = alpha for stationary signals
 *
 * 3. SPECTRAL EXPONENT (beta):
 *    - S(f) ~ 1/f^beta
 *    - beta = 0: White noise
 *    - beta = 1: Pink noise
 *    - beta = 2: Brownian noise
 *
 * 4. FRACTAL DIMENSION (D):
 *    - D = 2 - H for time series
 *    - D = 1: Smooth line
 *    - D = 2: Space-filling curve
 *    - Higher D = more complex/rough
 *
 * 5. LACUNARITY (Lambda):
 *    - Measures gap distribution at different scales
 *    - Lambda ~ 1: Homogeneous, uniform gaps
 *    - Lambda >> 1: Heterogeneous, clustered
 *
 * 6. MULTIFRACTAL SPECTRUM:
 *    - f(alpha): Singularity spectrum
 *    - Monofractal: Single Hurst exponent
 *    - Multifractal: Range of local Hurst exponents
 *
 * REFERENCES:
 * - Hurst (1951): Long-term storage capacity of reservoirs
 * - Peng et al. (1994): Mosaic organization of DNA nucleotides
 * - Kantelhardt et al. (2002): Multifractal detrended fluctuation analysis
 * - Mandelbrot (1982): The Fractal Geometry of Nature
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_FRACTAL_H
#define NIMCP_FRACTAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// NIMCP Export Macro
//=============================================================================

#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Configuration Constants
//=============================================================================

/** @brief Minimum samples for meaningful fractal analysis */
#define FRACTAL_MIN_SAMPLES           64

/** @brief Maximum samples for analysis (memory constraint) */
#define FRACTAL_MAX_SAMPLES           1048576

/** @brief Default minimum scale for analysis */
#define FRACTAL_DEFAULT_MIN_SCALE     4

/** @brief Default maximum scale (as fraction of N) */
#define FRACTAL_DEFAULT_MAX_SCALE_FRAC 0.25f

/** @brief Default number of scales to compute */
#define FRACTAL_DEFAULT_NUM_SCALES    20

/** @brief Default confidence threshold */
#define FRACTAL_DEFAULT_CONFIDENCE    0.95f

/** @brief Tolerance for pink noise classification */
#define FRACTAL_PINK_NOISE_TOLERANCE  0.15f

/** @brief Maximum multifractal q values */
#define FRACTAL_MAX_Q_VALUES          101

/** @brief DFA polynomial order for detrending */
#define FRACTAL_DFA_POLY_ORDER        1

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Fractal analysis result
 *
 * WHAT: Complete set of fractal metrics from analysis
 * WHY:  Characterize signal complexity and correlation structure
 * HOW:  Computed via multiple complementary methods
 */
typedef struct {
    float hurst_exponent;      /**< H: 0-1, 0.5=random, >0.5=persistent */
    float spectral_exponent;   /**< alpha: slope of log-log PSD (1.0 = pink) */
    float dfa_exponent;        /**< DFA alpha: 0.5=white, 1.0=pink, 1.5=brown */
    float fractal_dimension;   /**< D: box-counting dimension (~2-H for 1D) */
    float lacunarity;          /**< Lambda: gap distribution measure */
    float confidence;          /**< Statistical confidence in estimates */

    /* Additional diagnostic information */
    float hurst_r2;            /**< R^2 for Hurst log-log fit */
    float dfa_r2;              /**< R^2 for DFA log-log fit */
    float spectral_r2;         /**< R^2 for spectral log-log fit */
    uint32_t samples_analyzed; /**< Number of samples used */
    uint32_t scales_computed;  /**< Number of scales analyzed */
} fractal_result_t;

/**
 * @brief Multifractal spectrum
 *
 * WHAT: Generalized dimensions and singularity spectrum
 * WHY:  Characterize multifractal (non-uniform scaling) behavior
 * HOW:  Compute tau(q) for range of moment orders q
 *
 * RELATIONSHIPS:
 * - tau(q) = q*h(q) - 1: Mass exponent function
 * - h(q) = d(tau(q))/dq: Generalized Hurst exponent
 * - D(q) = tau(q)/(q-1): Generalized dimension (Renyi)
 * - f(alpha) = q*alpha - tau(q): Singularity spectrum (Legendre)
 * - alpha = d(tau(q))/dq: Singularity strength
 */
typedef struct {
    float* q_values;           /**< Moment orders (e.g., -5 to 5) */
    float* tau_q;              /**< Scaling exponents tau(q) */
    float* h_q;                /**< Generalized Hurst h(q) = (tau(q)+1)/q */
    float* D_q;                /**< Generalized dimension D(q) */
    float* f_alpha;            /**< Multifractal spectrum f(alpha) */
    float* alpha;              /**< Singularity strength */
    size_t spectrum_size;      /**< Number of q values computed */

    /* Summary statistics */
    float width;               /**< Spectrum width: max(alpha) - min(alpha) */
    float peak_alpha;          /**< alpha at max f(alpha) */
    float peak_f;              /**< Maximum f(alpha) value */
    float h_mono;              /**< Monofractal estimate: h(q=2) */
    float asymmetry;           /**< Left-right asymmetry of spectrum */
    bool is_multifractal;      /**< True if spectrum width > threshold */
} multifractal_spectrum_t;

/**
 * @brief Fractal analysis configuration
 *
 * WHAT: Parameters controlling fractal analysis algorithms
 * WHY:  Flexibility to tune for different signal types and accuracy needs
 * HOW:  Controls scale ranges, fitting options, and validation thresholds
 */
typedef struct {
    size_t min_scale;          /**< Minimum scale for analysis */
    size_t max_scale;          /**< Maximum scale (0 = auto from N/4) */
    size_t num_scales;         /**< Number of scales to compute */
    bool use_log_scales;       /**< Logarithmic (true) or linear scale spacing */
    float confidence_threshold; /**< Minimum R^2 for valid result */

    /* DFA-specific options */
    int dfa_poly_order;        /**< Polynomial order for DFA detrending (1-3) */
    bool dfa_remove_mean;      /**< Remove mean before integration */

    /* Spectral-specific options */
    bool spectral_use_welch;   /**< Use Welch method for PSD (more stable) */
    size_t spectral_window_size; /**< Window size for Welch (0 = auto) */
    float spectral_overlap;    /**< Welch overlap fraction (0.0-0.9) */

    /* Box-counting options */
    size_t box_min_size;       /**< Minimum box size for box-counting */
    size_t box_max_size;       /**< Maximum box size */

    /* Validation options */
    bool validate_crossover;   /**< Check for crossover in scaling */
    float crossover_threshold; /**< R^2 threshold for crossover detection */
} fractal_config_t;

/**
 * @brief Internal state for fractal computation
 */
typedef struct fractal_state_struct fractal_state_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default fractal analysis configuration
 *
 * WHAT: Returns sensible defaults for general-purpose fractal analysis
 * WHY:  Easy starting point without parameter tuning
 * HOW:  Values optimized for typical neural signal characteristics
 *
 * DEFAULT VALUES:
 * - min_scale = 4
 * - max_scale = 0 (auto: N/4)
 * - num_scales = 20
 * - use_log_scales = true
 * - confidence_threshold = 0.95
 * - dfa_poly_order = 1 (linear detrending)
 * - dfa_remove_mean = true
 * - spectral_use_welch = true
 * - spectral_window_size = 0 (auto)
 * - spectral_overlap = 0.5
 *
 * @return Default configuration
 */
NIMCP_EXPORT fractal_config_t fractal_config_default(void);

/**
 * @brief Validate fractal analysis configuration
 *
 * WHAT: Check if configuration parameters are in valid ranges
 * WHY:  Prevent invalid analysis or runtime errors
 * HOW:  Range checks on all parameters
 *
 * VALIDATION RULES:
 * - min_scale >= 4
 * - num_scales >= 4
 * - confidence_threshold in [0.5, 1.0]
 * - dfa_poly_order in [1, 3]
 * - spectral_overlap in [0.0, 0.9]
 *
 * @param config Configuration to validate
 * @return true if valid, false if invalid
 */
NIMCP_EXPORT bool fractal_config_validate(const fractal_config_t* config);

//=============================================================================
// Hurst Exponent (R/S Analysis)
//=============================================================================

/**
 * @brief Compute Hurst exponent using rescaled range (R/S) analysis
 *
 * WHAT: Classical method for estimating long-range dependence
 * WHY:  Measure persistence/anti-persistence in time series
 * HOW:  For each scale n, compute R(n)/S(n), fit log-log slope
 *
 * ALGORITHM:
 * 1. For each scale n from min_scale to max_scale:
 *    a. Divide series into segments of length n
 *    b. For each segment:
 *       - Compute mean m and standard deviation S
 *       - Compute cumulative deviation: Y(t) = sum(X(1:t) - m)
 *       - Compute range: R = max(Y) - min(Y)
 *    c. Average R/S over all segments
 * 2. Fit log(R/S) vs log(n) using linear regression
 * 3. Slope = Hurst exponent H
 *
 * INTERPRETATION:
 * - H = 0.5: Random walk (no long-range dependence)
 * - H > 0.5: Persistent (positive autocorrelation)
 * - H < 0.5: Anti-persistent (negative autocorrelation)
 *
 * @param samples Input signal samples
 * @param count Number of samples (>= FRACTAL_MIN_SAMPLES)
 * @param config Analysis configuration (NULL for defaults)
 * @param result Output: Hurst exponent and confidence metrics
 * @return 0 on success, negative error code on failure
 *
 * COMPLEXITY: O(N * num_scales)
 *
 * USAGE:
 * @code
 * float signal[1024];
 * fractal_result_t result;
 * fractal_hurst_rs(signal, 1024, NULL, &result);
 * printf("Hurst exponent: %.3f (R^2=%.3f)\n",
 *        result.hurst_exponent, result.hurst_r2);
 * @endcode
 */
NIMCP_EXPORT int fractal_hurst_rs(
    const float* samples,
    size_t count,
    const fractal_config_t* config,
    fractal_result_t* result
);

//=============================================================================
// Detrended Fluctuation Analysis (DFA)
//=============================================================================

/**
 * @brief Compute DFA exponent using detrended fluctuation analysis
 *
 * WHAT: Robust method for detecting long-range correlations
 * WHY:  More accurate than R/S for non-stationary signals with trends
 * HOW:  Integrate, detrend in windows, compute fluctuation, fit slope
 *
 * ALGORITHM:
 * 1. Integrate signal: y(k) = sum_{i=1}^{k} (x(i) - mean(x))
 * 2. For each scale n from min_scale to max_scale:
 *    a. Divide y into segments of length n
 *    b. For each segment:
 *       - Fit polynomial trend y_fit
 *       - Compute fluctuation: F^2 = mean((y - y_fit)^2)
 *    c. Average fluctuation: F(n) = sqrt(mean(F^2))
 * 3. Fit log(F) vs log(n) using linear regression
 * 4. Slope = DFA exponent alpha
 *
 * INTERPRETATION:
 * - alpha = 0.5: White noise (uncorrelated)
 * - alpha = 1.0: Pink noise (1/f, optimal for brain)
 * - alpha = 1.5: Brown noise (1/f^2, random walk)
 * - alpha > 1.5: Long-range correlated
 *
 * RELATION TO HURST:
 * - For stationary signals: H = alpha
 * - For non-stationary: alpha = H + 1
 *
 * @param samples Input signal samples
 * @param count Number of samples (>= FRACTAL_MIN_SAMPLES)
 * @param config Analysis configuration (NULL for defaults)
 * @param result Output: DFA exponent and confidence metrics
 * @return 0 on success, negative error code on failure
 *
 * COMPLEXITY: O(N * num_scales)
 *
 * USAGE:
 * @code
 * float signal[1024];
 * fractal_result_t result;
 * fractal_dfa(signal, 1024, NULL, &result);
 * if (fabs(result.dfa_exponent - 1.0f) < 0.1f) {
 *     printf("Signal exhibits pink noise characteristics\n");
 * }
 * @endcode
 */
NIMCP_EXPORT int fractal_dfa(
    const float* samples,
    size_t count,
    const fractal_config_t* config,
    fractal_result_t* result
);

//=============================================================================
// Spectral Exponent
//=============================================================================

/**
 * @brief Compute spectral exponent from power spectral density
 *
 * WHAT: Estimate 1/f^alpha exponent from frequency domain
 * WHY:  Direct measurement of power-law spectrum
 * HOW:  Compute PSD, fit line to log(P) vs log(f), slope = -alpha
 *
 * ALGORITHM:
 * 1. Compute power spectral density P(f) via FFT or Welch method
 * 2. Take logarithm: log(P) and log(f)
 * 3. Fit linear regression in log-log space
 * 4. Slope = -alpha (note sign: P(f) ~ 1/f^alpha)
 *
 * INTERPRETATION:
 * - alpha = 0: White noise (flat spectrum)
 * - alpha = 1: Pink noise (1/f)
 * - alpha = 2: Brown/Brownian noise (1/f^2)
 *
 * @param samples Input signal samples
 * @param count Number of samples (>= FRACTAL_MIN_SAMPLES)
 * @param result Output: Spectral exponent and confidence metrics
 * @return 0 on success, negative error code on failure
 *
 * COMPLEXITY: O(N log N) for FFT
 *
 * USAGE:
 * @code
 * float signal[1024];
 * fractal_result_t result;
 * fractal_spectral_exponent(signal, 1024, &result);
 * printf("Spectral exponent: %.3f\n", result.spectral_exponent);
 * @endcode
 */
NIMCP_EXPORT int fractal_spectral_exponent(
    const float* samples,
    size_t count,
    fractal_result_t* result
);

/**
 * @brief Compute spectral exponent with configuration
 *
 * WHAT: Extended version with configuration options
 * WHY:  Control PSD estimation method and frequency range
 * HOW:  Use Welch method for more stable estimates
 *
 * @param samples Input signal samples
 * @param count Number of samples
 * @param config Analysis configuration
 * @param result Output: Spectral exponent and metrics
 * @return 0 on success, negative error code on failure
 */
NIMCP_EXPORT int fractal_spectral_exponent_config(
    const float* samples,
    size_t count,
    const fractal_config_t* config,
    fractal_result_t* result
);

//=============================================================================
// Box-Counting Dimension
//=============================================================================

/**
 * @brief Compute box-counting (Minkowski-Bouligand) dimension
 *
 * WHAT: Estimate fractal dimension via box-counting method
 * WHY:  Measure geometric complexity of signal trajectory
 * HOW:  Count boxes at different scales, fit N(eps) ~ eps^(-D)
 *
 * ALGORITHM:
 * 1. Normalize signal to unit interval [0, 1]
 * 2. For each box size epsilon from max to min:
 *    a. Create grid with box size epsilon
 *    b. Count N(epsilon) = number of boxes containing signal
 * 3. Fit log(N) vs log(1/epsilon)
 * 4. Slope = fractal dimension D
 *
 * INTERPRETATION:
 * - D = 1: Smooth curve (Euclidean)
 * - D ~ 1.5: Typical fractal (e.g., coastline)
 * - D ~ 2: Space-filling, maximally rough
 *
 * RELATION TO HURST:
 * - For 1D time series: D = 2 - H
 *
 * @param samples Input signal samples
 * @param count Number of samples
 * @param config Analysis configuration (NULL for defaults)
 * @param result Output: Fractal dimension and metrics
 * @return 0 on success, negative error code on failure
 *
 * COMPLEXITY: O(N * num_scales)
 *
 * USAGE:
 * @code
 * float signal[1024];
 * fractal_result_t result;
 * fractal_box_dimension(signal, 1024, NULL, &result);
 * printf("Fractal dimension: %.3f\n", result.fractal_dimension);
 * @endcode
 */
NIMCP_EXPORT int fractal_box_dimension(
    const float* samples,
    size_t count,
    const fractal_config_t* config,
    fractal_result_t* result
);

//=============================================================================
// Lacunarity
//=============================================================================

/**
 * @brief Compute lacunarity at specified box size
 *
 * WHAT: Measure gap/hole distribution structure
 * WHY:  Distinguish between fractals with same dimension but different texture
 * HOW:  Lambda(r) = variance(mass) / mean(mass)^2 at scale r
 *
 * ALGORITHM:
 * 1. Create sliding window of size r
 * 2. Compute mass (sum) in each window position
 * 3. Calculate statistics:
 *    - mean_mass = E[mass]
 *    - var_mass = Var[mass]
 * 4. Lacunarity: Lambda(r) = var_mass / mean_mass^2 + 1
 *
 * INTERPRETATION:
 * - Lambda ~ 1: Homogeneous, uniform distribution
 * - Lambda >> 1: Heterogeneous, clustered, gappy
 * - Lambda decreases with scale for homogeneous systems
 *
 * @param samples Input signal samples
 * @param count Number of samples
 * @param box_size Box size for lacunarity calculation
 * @return Lacunarity value (>= 1.0), or -1.0 on error
 *
 * COMPLEXITY: O(N)
 *
 * USAGE:
 * @code
 * float signal[1024];
 * float lac = fractal_lacunarity(signal, 1024, 16);
 * printf("Lacunarity at scale 16: %.3f\n", lac);
 * @endcode
 */
NIMCP_EXPORT float fractal_lacunarity(
    const float* samples,
    size_t count,
    size_t box_size
);

/**
 * @brief Compute lacunarity curve over multiple scales
 *
 * WHAT: Lacunarity as function of box size
 * WHY:  Full characterization of spatial heterogeneity
 * HOW:  Compute lacunarity at each scale, analyze scaling behavior
 *
 * @param samples Input signal samples
 * @param count Number of samples
 * @param config Analysis configuration
 * @param scales Output: Array of scales used (must be pre-allocated)
 * @param lacunarities Output: Lacunarity at each scale (must be pre-allocated)
 * @param num_scales Number of scales to compute
 * @return 0 on success, negative error code on failure
 */
NIMCP_EXPORT int fractal_lacunarity_curve(
    const float* samples,
    size_t count,
    const fractal_config_t* config,
    float* scales,
    float* lacunarities,
    size_t num_scales
);

//=============================================================================
// Multifractal Analysis
//=============================================================================

/**
 * @brief Compute full multifractal spectrum via MFDFA
 *
 * WHAT: Multifractal detrended fluctuation analysis
 * WHY:  Detect non-uniform scaling, multiple Hurst exponents
 * HOW:  Compute generalized Hurst h(q) for range of q values
 *
 * ALGORITHM (Multifractal DFA):
 * 1. Integrate signal as in standard DFA
 * 2. For each scale n:
 *    a. Divide into segments, fit polynomial trends
 *    b. Compute fluctuation F^2(s,n) for each segment s
 * 3. For each moment order q:
 *    a. Compute q-th order fluctuation:
 *       F_q(n) = {1/N_s * sum_s [F^2(s,n)]^(q/2)}^(1/q)
 *    b. Fit log(F_q) vs log(n) to get h(q)
 * 4. Derive multifractal quantities:
 *    - tau(q) = q*h(q) - 1
 *    - D(q) = tau(q)/(q-1)
 *    - alpha = d(tau)/dq, f(alpha) = q*alpha - tau(q)
 *
 * INTERPRETATION:
 * - Monofractal: h(q) = constant for all q
 * - Multifractal: h(q) varies with q
 * - Spectrum width: Degree of multifractality
 * - Asymmetry: Left-skewed = small fluctuations dominant
 *
 * @param samples Input signal samples
 * @param count Number of samples (>= FRACTAL_MIN_SAMPLES)
 * @param q_min Minimum moment order (e.g., -5)
 * @param q_max Maximum moment order (e.g., +5)
 * @param q_steps Number of q values to compute
 * @param spectrum Output: Allocated multifractal spectrum (caller frees)
 * @return 0 on success, negative error code on failure
 *
 * COMPLEXITY: O(N * num_scales * q_steps)
 *
 * USAGE:
 * @code
 * float signal[1024];
 * multifractal_spectrum_t* mf;
 * fractal_multifractal_spectrum(signal, 1024, -5.0f, 5.0f, 21, &mf);
 * printf("Spectrum width: %.3f\n", mf->width);
 * printf("Is multifractal: %s\n", mf->is_multifractal ? "yes" : "no");
 * multifractal_spectrum_destroy(mf);
 * @endcode
 */
NIMCP_EXPORT int fractal_multifractal_spectrum(
    const float* samples,
    size_t count,
    float q_min,
    float q_max,
    size_t q_steps,
    multifractal_spectrum_t** spectrum
);

/**
 * @brief Destroy multifractal spectrum and free resources
 *
 * WHAT: Free memory allocated for multifractal spectrum
 * WHY:  Prevent memory leaks
 * HOW:  Free all internal arrays and structure
 *
 * @param spectrum Spectrum to destroy (can be NULL)
 */
NIMCP_EXPORT void multifractal_spectrum_destroy(multifractal_spectrum_t* spectrum);

//=============================================================================
// Validation Functions
//=============================================================================

/**
 * @brief Check if signal exhibits pink noise characteristics
 *
 * WHAT: Validate that signal is approximately pink (1/f) noise
 * WHY:  Verify system operates at criticality, validate noise generation
 * HOW:  Check if alpha ~ 1.0 within tolerance
 *
 * Uses DFA as primary method (most robust for typical signals).
 *
 * @param samples Input signal samples
 * @param count Number of samples
 * @param tolerance Acceptable deviation from alpha=1.0 (e.g., 0.1)
 * @return true if pink noise, false otherwise or on error
 *
 * USAGE:
 * @code
 * if (fractal_is_pink_noise(signal, 1024, 0.15f)) {
 *     printf("Signal exhibits pink noise characteristics\n");
 * }
 * @endcode
 */
NIMCP_EXPORT bool fractal_is_pink_noise(
    const float* samples,
    size_t count,
    float tolerance
);

/**
 * @brief Check if signal is self-similar (scale-invariant)
 *
 * WHAT: Validate that signal exhibits scale-invariance
 * WHY:  Detect fractal behavior across multiple scales
 * HOW:  Check for linear scaling in log-log plots over min_scales
 *
 * A signal is considered self-similar if:
 * 1. DFA shows consistent power-law scaling
 * 2. R^2 of log-log fit exceeds confidence threshold
 * 3. Scaling holds over at least min_scales scales
 *
 * @param samples Input signal samples
 * @param count Number of samples
 * @param min_scales Minimum number of scales for valid scaling (e.g., 8)
 * @return true if self-similar, false otherwise or on error
 *
 * USAGE:
 * @code
 * if (fractal_is_self_similar(signal, 1024, 10)) {
 *     printf("Signal is self-similar across 10+ scales\n");
 * }
 * @endcode
 */
NIMCP_EXPORT bool fractal_is_self_similar(
    const float* samples,
    size_t count,
    size_t min_scales
);

/**
 * @brief Validate signal quality for fractal analysis
 *
 * WHAT: Check if signal is suitable for fractal analysis
 * WHY:  Prevent unreliable results from poor quality data
 * HOW:  Check for sufficient samples, variance, stationarity
 *
 * VALIDATION CHECKS:
 * - Minimum sample count
 * - Non-zero variance
 * - No constant segments
 * - No NaN/Inf values
 *
 * @param samples Input signal samples
 * @param count Number of samples
 * @return true if valid, false if unsuitable
 */
NIMCP_EXPORT bool fractal_validate_signal(
    const float* samples,
    size_t count
);

//=============================================================================
// Comprehensive Analysis
//=============================================================================

/**
 * @brief Perform comprehensive fractal analysis
 *
 * WHAT: Compute all fractal metrics in single call
 * WHY:  Convenience function for complete characterization
 * HOW:  Runs Hurst, DFA, spectral, box-counting, lacunarity
 *
 * @param samples Input signal samples
 * @param count Number of samples
 * @param config Analysis configuration (NULL for defaults)
 * @param result Output: Complete fractal result
 * @return 0 on success, negative error code on failure
 *
 * USAGE:
 * @code
 * float signal[1024];
 * fractal_config_t config = fractal_config_default();
 * fractal_result_t result;
 * fractal_analyze(signal, 1024, &config, &result);
 * fractal_result_print(&result);
 * @endcode
 */
NIMCP_EXPORT int fractal_analyze(
    const float* samples,
    size_t count,
    const fractal_config_t* config,
    fractal_result_t* result
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Print fractal result for debugging
 *
 * WHAT: Human-readable output of fractal metrics
 * WHY:  Debugging and verification
 * HOW:  Format and print all fields
 *
 * @param result Fractal result to print
 */
NIMCP_EXPORT void fractal_result_print(const fractal_result_t* result);

/**
 * @brief Print multifractal spectrum for debugging
 *
 * WHAT: Human-readable output of multifractal analysis
 * WHY:  Debugging and verification
 * HOW:  Format and print spectrum details
 *
 * @param spectrum Multifractal spectrum to print
 */
NIMCP_EXPORT void multifractal_spectrum_print(const multifractal_spectrum_t* spectrum);

/**
 * @brief Estimate required sample count for target confidence
 *
 * WHAT: Suggest minimum samples for reliable analysis
 * WHY:  Help users prepare adequate data
 * HOW:  Based on statistical requirements for given confidence
 *
 * @param confidence_target Desired confidence level (0.9-0.99)
 * @param method Analysis method (0=DFA, 1=Hurst, 2=Spectral)
 * @return Suggested minimum sample count
 */
NIMCP_EXPORT size_t fractal_estimate_sample_requirement(
    float confidence_target,
    int method
);

/**
 * @brief Convert between fractal exponents
 *
 * WHAT: Convert Hurst exponent to/from other metrics
 * WHY:  Convenience for comparing different characterizations
 * HOW:  Apply mathematical relationships
 *
 * RELATIONSHIPS:
 * - Hurst H -> DFA alpha: alpha = H (stationary)
 * - Hurst H -> Spectral beta: beta = 2*H - 1
 * - Hurst H -> Fractal dim D: D = 2 - H
 *
 * @param hurst Input Hurst exponent
 * @param dfa_out Output DFA exponent (can be NULL)
 * @param spectral_out Output spectral exponent (can be NULL)
 * @param dimension_out Output fractal dimension (can be NULL)
 */
NIMCP_EXPORT void fractal_convert_exponents(
    float hurst,
    float* dfa_out,
    float* spectral_out,
    float* dimension_out
);

/**
 * @brief Get noise type classification string
 *
 * WHAT: Classify signal based on DFA exponent
 * WHY:  Human-readable interpretation
 * HOW:  Map exponent ranges to noise types
 *
 * @param dfa_exponent DFA alpha exponent
 * @return String classification ("white", "pink", "brown", etc.)
 */
NIMCP_EXPORT const char* fractal_classify_noise(float dfa_exponent);

//=============================================================================
// Error Codes
//=============================================================================

/** @brief Success */
#define FRACTAL_OK                    0

/** @brief Null pointer argument */
#define FRACTAL_ERROR_NULL_PTR       -1

/** @brief Insufficient samples */
#define FRACTAL_ERROR_INSUFFICIENT   -2

/** @brief Invalid configuration */
#define FRACTAL_ERROR_INVALID_CONFIG -3

/** @brief Memory allocation failed */
#define FRACTAL_ERROR_ALLOC          -4

/** @brief Computation failed (singular matrix, etc.) */
#define FRACTAL_ERROR_COMPUTE        -5

/** @brief Signal quality too poor */
#define FRACTAL_ERROR_QUALITY        -6

/** @brief Invalid parameter value */
#define FRACTAL_ERROR_PARAM          -7

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FRACTAL_H */
