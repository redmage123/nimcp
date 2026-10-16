//=============================================================================
// nimcp_timeseries.h - Time Series Analysis Module for NIMCP
//=============================================================================
/**
 * @file nimcp_timeseries.h
 * @brief Comprehensive time series analysis with GPU acceleration
 *
 * WHAT: Complete time series analysis toolkit including autocorrelation,
 *       spectral analysis, ARIMA modeling, stationarity tests, smoothing,
 *       causality analysis, and change point detection.
 *
 * WHY:  Neural systems exhibit complex temporal dynamics - from EEG oscillations
 *       to spike train patterns. This module provides the mathematical foundation
 *       for analyzing temporal structure in neural data, essential for:
 *       - Neural oscillation analysis (alpha, beta, gamma rhythms)
 *       - Spike train correlation and Granger causality
 *       - ERP component extraction via filtering
 *       - Neural prediction and forecasting
 *       - Detecting regime changes in brain states
 *
 * HOW:  C99 implementation with GPU acceleration for FFT-based operations,
 *       NaN-aware algorithms for missing data, and irregular sampling support.
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Temporal Structure in Neural Systems:
 *   +--------------------------------------------------------------------+
 *   |                                                                    |
 *   |  EEG/LFP Analysis:                                                 |
 *   |    Spectral power: theta (4-8Hz), alpha (8-13Hz), beta (13-30Hz)  |
 *   |    Phase-amplitude coupling: cross-frequency interactions          |
 *   |    Coherence: functional connectivity between brain regions        |
 *   |                                                                    |
 *   |  Spike Train Analysis:                                             |
 *   |    Autocorrelation: reveals oscillatory firing patterns            |
 *   |    Cross-correlation: detects functional connections               |
 *   |    Granger causality: directional information flow                 |
 *   |                                                                    |
 *   |  Brain State Dynamics:                                             |
 *   |    Change point detection: sleep stage transitions                 |
 *   |    Stationarity: stability of neural dynamics                      |
 *   |    ARIMA: predictive models of neural activity                     |
 *   |                                                                    |
 *   +--------------------------------------------------------------------+
 *
 * FEATURES:
 * - Autocorrelation (ACF, PACF, Ljung-Box, Durbin-Watson)
 * - Spectral analysis (periodogram, Welch, multitaper, coherence)
 * - ARIMA modeling with automatic order selection
 * - Stationarity tests (ADF, KPSS, Phillips-Perron)
 * - Smoothing filters (MA, exponential, Holt-Winters, Kalman, Savitzky-Golay)
 * - Causality analysis (Granger, transfer entropy, CCM)
 * - Change point detection (CUSUM, Pettitt, binary segmentation)
 * - Full NaN/missing value support
 * - Irregular time series interpolation
 * - GPU acceleration for FFT-based operations
 *
 * PERFORMANCE:
 * - FFT operations: O(n log n), GPU accelerated
 * - Autocorrelation (n=10000): ~5ms CPU, <1ms GPU
 * - ARIMA fitting: <100ms for typical orders
 * - Welch PSD (n=100000): <50ms GPU
 *
 * THREAD SAFETY:
 * - All functions are reentrant
 * - GPU operations use context streams
 * - No global mutable state
 *
 * @author NIMCP Development Team
 * @date 2026-01-30
 * @version 1.0.0
 */

#ifndef NIMCP_TIMESERIES_H
#define NIMCP_TIMESERIES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <float.h>

// Export macro
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum lag for autocorrelation functions */
#define NIMCP_TS_MAX_LAG 10000

/** Default number of FFT points for spectral estimation */
#define NIMCP_TS_DEFAULT_NFFT 1024

/** Default Welch segment overlap (50%) */
#define NIMCP_TS_DEFAULT_OVERLAP 0.5f

/** Maximum ARIMA order (p, d, q) */
#define NIMCP_TS_MAX_ARIMA_ORDER 10

/** Default number of tapers for multitaper */
#define NIMCP_TS_DEFAULT_TAPERS 7

/** Tolerance for numerical comparisons */
#define NIMCP_TS_EPSILON 1e-10

/** Maximum iterations for fitting algorithms */
#define NIMCP_TS_MAX_ITERATIONS 1000

//=============================================================================
// Error Codes
//=============================================================================

/**
 * @brief Result codes for time series operations
 */
typedef enum nimcp_ts_result {
    NIMCP_TS_OK = 0,                  /**< Success */
    NIMCP_TS_ERROR_NULL = -1,         /**< NULL pointer argument */
    NIMCP_TS_ERROR_SIZE = -2,         /**< Invalid size (n=0 or too small) */
    NIMCP_TS_ERROR_MEMORY = -3,       /**< Memory allocation failed */
    NIMCP_TS_ERROR_PARAMS = -4,       /**< Invalid parameters */
    NIMCP_TS_ERROR_CONVERGE = -5,     /**< Algorithm did not converge */
    NIMCP_TS_ERROR_SINGULAR = -6,     /**< Singular matrix in computation */
    NIMCP_TS_ERROR_NON_STATIONARY = -7, /**< Series is non-stationary */
    NIMCP_TS_ERROR_GPU = -8,          /**< GPU operation failed */
    NIMCP_TS_ERROR_FFT = -9,          /**< FFT operation failed */
    NIMCP_TS_ERROR_NOT_INIT = -10     /**< Module not initialized */
} nimcp_ts_result_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Window function types for spectral analysis
 */
typedef enum nimcp_ts_window {
    NIMCP_TS_WINDOW_RECTANGULAR = 0, /**< Rectangular (boxcar) */
    NIMCP_TS_WINDOW_HANN,            /**< Hann window */
    NIMCP_TS_WINDOW_HAMMING,         /**< Hamming window */
    NIMCP_TS_WINDOW_BLACKMAN,        /**< Blackman window */
    NIMCP_TS_WINDOW_BLACKMAN_HARRIS, /**< Blackman-Harris window */
    NIMCP_TS_WINDOW_KAISER,          /**< Kaiser window (requires beta param) */
    NIMCP_TS_WINDOW_GAUSSIAN,        /**< Gaussian window (requires sigma param) */
    NIMCP_TS_WINDOW_TUKEY,           /**< Tukey (tapered cosine) window */
    NIMCP_TS_WINDOW_SLEPIAN          /**< Slepian (DPSS) for multitaper */
} nimcp_ts_window_t;

/**
 * @brief Detrending methods
 */
typedef enum nimcp_ts_detrend {
    NIMCP_TS_DETREND_NONE = 0,       /**< No detrending */
    NIMCP_TS_DETREND_MEAN,           /**< Remove mean (constant) */
    NIMCP_TS_DETREND_LINEAR,         /**< Remove linear trend */
    NIMCP_TS_DETREND_POLYNOMIAL      /**< Remove polynomial trend */
} nimcp_ts_detrend_t;

/**
 * @brief Configuration for time series module
 */
typedef struct nimcp_ts_config {
    bool use_gpu;                    /**< Use GPU acceleration if available */
    bool handle_nan;                 /**< Handle NaN values gracefully */
    uint32_t default_nfft;           /**< Default FFT size */
    nimcp_ts_window_t default_window; /**< Default window function */
    nimcp_ts_detrend_t default_detrend; /**< Default detrending method */
    float confidence_level;          /**< Default confidence level (e.g., 0.95) */
    uint32_t max_iterations;         /**< Max iterations for fitting */
    float convergence_tol;           /**< Convergence tolerance */
} nimcp_ts_config_t;

//=============================================================================
// Result Structures
//=============================================================================

/**
 * @brief Autocorrelation function result
 */
typedef struct nimcp_acf_result {
    float* acf;              /**< ACF values [max_lag+1] */
    float* confidence_upper; /**< Upper confidence bound [max_lag+1] */
    float* confidence_lower; /**< Lower confidence bound [max_lag+1] */
    uint32_t max_lag;        /**< Maximum lag computed */
    uint32_t n;              /**< Original series length */
    float confidence_level;  /**< Confidence level used */
} nimcp_acf_result_t;

/**
 * @brief Partial autocorrelation function result
 */
typedef struct nimcp_pacf_result {
    float* pacf;             /**< PACF values [max_lag+1] */
    float* confidence_upper; /**< Upper confidence bound [max_lag+1] */
    float* confidence_lower; /**< Lower confidence bound [max_lag+1] */
    uint32_t max_lag;        /**< Maximum lag computed */
    uint32_t n;              /**< Original series length */
    float confidence_level;  /**< Confidence level used */
} nimcp_pacf_result_t;

/**
 * @brief Ljung-Box test result
 */
typedef struct nimcp_ljung_box_result {
    float statistic;         /**< Test statistic Q */
    float p_value;           /**< P-value */
    uint32_t lags;           /**< Number of lags tested */
    float df;                /**< Degrees of freedom */
    bool significant;        /**< true if significant autocorrelation detected */
} nimcp_ljung_box_result_t;

/**
 * @brief Spectral density result
 */
typedef struct nimcp_psd_result {
    float* frequencies;      /**< Frequency values [n_freqs] */
    float* power;            /**< Power spectral density [n_freqs] */
    float* confidence_lower; /**< Lower confidence bound [n_freqs] (optional) */
    float* confidence_upper; /**< Upper confidence bound [n_freqs] (optional) */
    uint32_t n_freqs;        /**< Number of frequency points */
    float fs;                /**< Sampling frequency */
    float df;                /**< Frequency resolution */
    float total_power;       /**< Total integrated power */
} nimcp_psd_result_t;

/**
 * @brief Coherence result (between two series)
 */
typedef struct nimcp_coherence_result {
    float* frequencies;      /**< Frequency values [n_freqs] */
    float* coherence;        /**< Magnitude squared coherence [n_freqs] */
    float* phase;            /**< Phase spectrum [n_freqs] (radians) */
    float* confidence;       /**< Confidence threshold [n_freqs] */
    uint32_t n_freqs;        /**< Number of frequency points */
    float fs;                /**< Sampling frequency */
} nimcp_coherence_result_t;

/**
 * @brief Cross-spectral density result
 */
typedef struct nimcp_cross_spectrum_result {
    float* frequencies;      /**< Frequency values [n_freqs] */
    float* csd_real;         /**< Real part of CSD [n_freqs] */
    float* csd_imag;         /**< Imaginary part of CSD [n_freqs] */
    float* magnitude;        /**< Magnitude [n_freqs] */
    float* phase;            /**< Phase [n_freqs] (radians) */
    uint32_t n_freqs;        /**< Number of frequency points */
    float fs;                /**< Sampling frequency */
} nimcp_cross_spectrum_result_t;

/**
 * @brief ARIMA model structure
 */
typedef struct nimcp_arima_model {
    uint32_t p;              /**< AR order */
    uint32_t d;              /**< Differencing order */
    uint32_t q;              /**< MA order */
    float* ar_coefs;         /**< AR coefficients [p] */
    float* ma_coefs;         /**< MA coefficients [q] */
    float intercept;         /**< Model intercept (constant term) */
    float sigma2;            /**< Innovation variance */
    float aic;               /**< Akaike Information Criterion */
    float bic;               /**< Bayesian Information Criterion */
    float aicc;              /**< Corrected AIC (for small samples) */
    float log_likelihood;    /**< Log-likelihood */
    uint32_t n_obs;          /**< Number of observations used */
    bool fitted;             /**< Whether model is fitted */
} nimcp_arima_model_t;

/**
 * @brief Stationarity test result
 */
typedef struct nimcp_stationarity_result {
    float statistic;         /**< Test statistic */
    float p_value;           /**< P-value */
    float critical_1pct;     /**< 1% critical value */
    float critical_5pct;     /**< 5% critical value */
    float critical_10pct;    /**< 10% critical value */
    uint32_t n_lags;         /**< Number of lags used */
    bool is_stationary;      /**< true if stationary at 5% level */
    const char* test_name;   /**< Name of test performed */
} nimcp_stationarity_result_t;

/**
 * @brief Granger causality test result
 */
typedef struct nimcp_granger_result {
    float f_statistic;       /**< F-statistic */
    float p_value;           /**< P-value */
    float ssr_reduced;       /**< SSR of reduced model */
    float ssr_full;          /**< SSR of full model */
    uint32_t df_num;         /**< Numerator degrees of freedom */
    uint32_t df_denom;       /**< Denominator degrees of freedom */
    uint32_t max_lag;        /**< Maximum lag tested */
    bool significant;        /**< true if X Granger-causes Y */
} nimcp_granger_result_t;

/**
 * @brief Change point detection result
 */
typedef struct nimcp_changepoint_result {
    uint32_t* locations;     /**< Change point indices [n_points] */
    float* statistics;       /**< Test statistics at each point [n_points] */
    uint32_t n_points;       /**< Number of change points detected */
    float threshold;         /**< Detection threshold used */
    float confidence;        /**< Confidence level */
} nimcp_changepoint_result_t;

/**
 * @brief Kalman filter state
 */
typedef struct nimcp_kalman_state {
    float* state;            /**< Current state estimate [state_dim] */
    float* covariance;       /**< State covariance [state_dim x state_dim] */
    float* F;                /**< State transition matrix [state_dim x state_dim] */
    float* H;                /**< Observation matrix [obs_dim x state_dim] */
    float* Q;                /**< Process noise covariance [state_dim x state_dim] */
    float* R;                /**< Observation noise covariance [obs_dim x obs_dim] */
    uint32_t state_dim;      /**< State dimension */
    uint32_t obs_dim;        /**< Observation dimension */
    bool initialized;        /**< Whether filter is initialized */
} nimcp_kalman_state_t;

/**
 * @brief Holt-Winters model parameters
 */
typedef struct nimcp_hw_params {
    float alpha;             /**< Level smoothing parameter */
    float beta;              /**< Trend smoothing parameter */
    float gamma;             /**< Seasonal smoothing parameter */
    uint32_t period;         /**< Seasonal period */
    bool additive;           /**< true for additive, false for multiplicative */
    float* level;            /**< Level components [n] */
    float* trend;            /**< Trend components [n] */
    float* seasonal;         /**< Seasonal components [period] */
    bool fitted;             /**< Whether model is fitted */
} nimcp_hw_params_t;

//=============================================================================
// Module Initialization
//=============================================================================

/**
 * @brief Get default configuration
 * @return Default time series configuration
 */
NIMCP_EXPORT nimcp_ts_config_t nimcp_ts_default_config(void);

/**
 * @brief Initialize the time series module
 * @param config Configuration (NULL for defaults)
 * @return NIMCP_TS_OK on success
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_ts_init(const nimcp_ts_config_t* config);

/**
 * @brief Shutdown the time series module
 */
NIMCP_EXPORT void nimcp_ts_shutdown(void);

/**
 * @brief Check if time series module is initialized
 * @return true if initialized
 */
NIMCP_EXPORT bool nimcp_ts_is_initialized(void);

//=============================================================================
// Autocorrelation Analysis
//=============================================================================

/**
 * @brief Compute autocorrelation at a specific lag
 *
 * @param x Time series data
 * @param n Number of observations
 * @param lag Lag value (0 = autocorrelation with itself = 1.0)
 * @return Autocorrelation coefficient, or NAN on error
 *
 * FORMULA: r(k) = Σ(x_t - x_bar)(x_{t+k} - x_bar) / Σ(x_t - x_bar)^2
 *
 * PERFORMANCE: O(n) for single lag
 */
NIMCP_EXPORT float nimcp_ts_autocorrelation(const float* x, uint32_t n, uint32_t lag);

/**
 * @brief Compute full autocorrelation function (ACF)
 *
 * @param x Time series data
 * @param n Number of observations
 * @param max_lag Maximum lag to compute (0 = auto = n-1)
 * @param confidence_level Confidence level for bounds (e.g., 0.95)
 * @param result Output ACF result (caller allocated)
 * @return NIMCP_TS_OK on success
 *
 * OUTPUT ARRAYS: Caller must pre-allocate result arrays of size (max_lag+1)
 *
 * NEUROSCIENCE: ACF reveals periodicity in neural signals - oscillatory
 * activity appears as decaying sinusoidal ACF patterns.
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_ts_acf(
    const float* x,
    uint32_t n,
    uint32_t max_lag,
    float confidence_level,
    nimcp_acf_result_t* result
);

/**
 * @brief Compute partial autocorrelation function (PACF)
 *
 * @param x Time series data
 * @param n Number of observations
 * @param max_lag Maximum lag to compute
 * @param confidence_level Confidence level for bounds
 * @param result Output PACF result (caller allocated)
 * @return NIMCP_TS_OK on success
 *
 * METHOD: Uses Durbin-Levinson algorithm for efficient computation.
 *
 * INTERPRETATION: PACF at lag k gives correlation between x_t and x_{t-k}
 * after removing effects of intermediate lags. Useful for AR order selection.
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_ts_pacf(
    const float* x,
    uint32_t n,
    uint32_t max_lag,
    float confidence_level,
    nimcp_pacf_result_t* result
);

/**
 * @brief Ljung-Box test for autocorrelation
 *
 * @param x Time series data (typically residuals from a fitted model)
 * @param n Number of observations
 * @param lags Number of lags to test
 * @param result Output test result
 * @return NIMCP_TS_OK on success
 *
 * HYPOTHESES:
 *   H0: No autocorrelation (white noise)
 *   H1: Autocorrelation present
 *
 * FORMULA: Q = n(n+2) Σ_{k=1}^h r_k^2 / (n-k) ~ chi^2(h)
 *
 * USE: Test whether ARIMA residuals are white noise.
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_ts_ljung_box(
    const float* x,
    uint32_t n,
    uint32_t lags,
    nimcp_ljung_box_result_t* result
);

/**
 * @brief Compute Durbin-Watson statistic
 *
 * @param residuals Model residuals
 * @param n Number of residuals
 * @return Durbin-Watson statistic d in [0, 4], or NAN on error
 *
 * INTERPRETATION:
 *   d ≈ 2: No autocorrelation
 *   d < 2: Positive autocorrelation
 *   d > 2: Negative autocorrelation
 *
 * FORMULA: d = Σ(e_t - e_{t-1})^2 / Σe_t^2
 */
NIMCP_EXPORT float nimcp_ts_durbin_watson(const float* residuals, uint32_t n);

//=============================================================================
// Spectral Analysis
//=============================================================================

/**
 * @brief Compute raw periodogram
 *
 * @param x Time series data
 * @param n Number of observations
 * @param fs Sampling frequency (Hz)
 * @param nfft FFT size (0 = next power of 2 >= n)
 * @param window Window function type
 * @param detrend Detrending method
 * @param result Output PSD result (caller must free with nimcp_psd_free)
 * @return NIMCP_TS_OK on success
 *
 * FORMULA: P(f) = (1/n)|X(f)|^2 where X(f) = FFT(x)
 *
 * GPU: Uses cuFFT when GPU acceleration is enabled.
 *
 * NOTE: Raw periodogram has high variance. Use Welch or multitaper for
 * better spectral estimates.
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_ts_periodogram(
    const float* x,
    uint32_t n,
    float fs,
    uint32_t nfft,
    nimcp_ts_window_t window,
    nimcp_ts_detrend_t detrend,
    nimcp_psd_result_t* result
);

/**
 * @brief Welch's power spectral density estimation
 *
 * @param x Time series data
 * @param n Number of observations
 * @param fs Sampling frequency (Hz)
 * @param segment_length Segment length (0 = n/8)
 * @param overlap Overlap fraction in [0, 1) (0.5 = 50% overlap)
 * @param nfft FFT size per segment
 * @param window Window function type
 * @param detrend Detrending method
 * @param result Output PSD result (caller must free with nimcp_psd_free)
 * @return NIMCP_TS_OK on success
 *
 * METHOD: Averages periodograms of overlapping segments to reduce variance.
 *
 * RECOMMENDATION:
 *   - segment_length = n/8 for good frequency resolution
 *   - overlap = 0.5 for Hann window (optimal)
 *   - nfft = 2 * segment_length for zero-padding
 *
 * GPU: Batched FFT acceleration for all segments.
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_ts_welch_psd(
    const float* x,
    uint32_t n,
    float fs,
    uint32_t segment_length,
    float overlap,
    uint32_t nfft,
    nimcp_ts_window_t window,
    nimcp_ts_detrend_t detrend,
    nimcp_psd_result_t* result
);

/**
 * @brief Multitaper spectral density estimation
 *
 * @param x Time series data
 * @param n Number of observations
 * @param fs Sampling frequency (Hz)
 * @param nw Time-bandwidth product (typically 2-4)
 * @param n_tapers Number of tapers (typically 2*nw - 1)
 * @param nfft FFT size
 * @param result Output PSD result (caller must free)
 * @return NIMCP_TS_OK on success
 *
 * METHOD: Uses Slepian sequences (DPSS) as tapers. Provides optimal
 * bias-variance tradeoff for spectral estimation.
 *
 * PARAMETERS:
 *   nw = 3, n_tapers = 5 is a good default
 *   Higher nw gives more smoothing but less frequency resolution
 *
 * NEUROSCIENCE: Multitaper is gold standard for EEG/MEG spectral analysis
 * due to its low bias and good frequency resolution.
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_ts_multitaper_psd(
    const float* x,
    uint32_t n,
    float fs,
    float nw,
    uint32_t n_tapers,
    uint32_t nfft,
    nimcp_psd_result_t* result
);

/**
 * @brief Compute spectral coherence between two time series
 *
 * @param x First time series
 * @param y Second time series
 * @param n Number of observations (must be same for both)
 * @param fs Sampling frequency (Hz)
 * @param segment_length Segment length for Welch estimation
 * @param overlap Overlap fraction
 * @param nfft FFT size
 * @param window Window function
 * @param result Output coherence result (caller must free)
 * @return NIMCP_TS_OK on success
 *
 * FORMULA: Coh(f) = |S_xy(f)|^2 / (S_xx(f) * S_yy(f))
 *
 * INTERPRETATION: Coherence measures linear correlation at each frequency.
 *   Coh ≈ 1: Strong linear relationship at that frequency
 *   Coh ≈ 0: No linear relationship
 *
 * NEUROSCIENCE: Coherence quantifies functional connectivity between
 * brain regions at specific frequency bands.
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_ts_coherence(
    const float* x,
    const float* y,
    uint32_t n,
    float fs,
    uint32_t segment_length,
    float overlap,
    uint32_t nfft,
    nimcp_ts_window_t window,
    nimcp_coherence_result_t* result
);

/**
 * @brief Compute cross-spectral density between two time series
 *
 * @param x First time series
 * @param y Second time series
 * @param n Number of observations
 * @param fs Sampling frequency (Hz)
 * @param segment_length Segment length
 * @param overlap Overlap fraction
 * @param nfft FFT size
 * @param window Window function
 * @param result Output cross-spectrum result (caller must free)
 * @return NIMCP_TS_OK on success
 *
 * FORMULA: S_xy(f) = conj(X(f)) * Y(f) / n
 *
 * The phase of CSD indicates time delay between signals at each frequency.
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_ts_cross_spectrum(
    const float* x,
    const float* y,
    uint32_t n,
    float fs,
    uint32_t segment_length,
    float overlap,
    uint32_t nfft,
    nimcp_ts_window_t window,
    nimcp_cross_spectrum_result_t* result
);

/**
 * @brief Compute spectral entropy
 *
 * @param x Time series data
 * @param n Number of observations
 * @param fs Sampling frequency
 * @param nfft FFT size
 * @param normalize true to normalize by log(n_freqs) to [0,1] range
 * @return Spectral entropy, or NAN on error
 *
 * FORMULA: H = -Σ P(f) * log2(P(f)) where P(f) = S(f) / Σ S(f)
 *
 * INTERPRETATION:
 *   Low entropy: Power concentrated at few frequencies (rhythmic)
 *   High entropy: Power spread across frequencies (noise-like)
 *
 * NEUROSCIENCE: Spectral entropy distinguishes rhythmic brain activity
 * from irregular patterns - useful for anesthesia depth monitoring.
 */
NIMCP_EXPORT float nimcp_ts_spectral_entropy(
    const float* x,
    uint32_t n,
    float fs,
    uint32_t nfft,
    bool normalize
);

/**
 * @brief Free PSD result memory
 */
NIMCP_EXPORT void nimcp_psd_free(nimcp_psd_result_t* result);

/**
 * @brief Free coherence result memory
 */
NIMCP_EXPORT void nimcp_coherence_free(nimcp_coherence_result_t* result);

/**
 * @brief Free cross-spectrum result memory
 */
NIMCP_EXPORT void nimcp_cross_spectrum_free(nimcp_cross_spectrum_result_t* result);

//=============================================================================
// ARIMA Modeling
//=============================================================================

/**
 * @brief Create ARIMA model structure
 *
 * @param p AR order
 * @param d Differencing order
 * @param q MA order
 * @return ARIMA model (caller must free with nimcp_arima_destroy)
 */
NIMCP_EXPORT nimcp_arima_model_t* nimcp_arima_create(uint32_t p, uint32_t d, uint32_t q);

/**
 * @brief Destroy ARIMA model
 *
 * @param model ARIMA model to destroy (can be NULL)
 */
NIMCP_EXPORT void nimcp_arima_destroy(nimcp_arima_model_t* model);

/**
 * @brief Fit ARIMA(p,d,q) model to time series
 *
 * @param x Time series data
 * @param n Number of observations
 * @param model ARIMA model with pre-set orders
 * @param include_intercept Include intercept term
 * @return NIMCP_TS_OK on success
 *
 * METHOD: Maximum likelihood estimation via conditional sum of squares
 * initialization followed by exact MLE optimization.
 *
 * REQUIREMENTS:
 *   - n > p + q + 10 (sufficient observations)
 *   - Series should be stationary after differencing
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_arima_fit(
    const float* x,
    uint32_t n,
    nimcp_arima_model_t* model,
    bool include_intercept
);

/**
 * @brief Forecast future values using fitted ARIMA model
 *
 * @param model Fitted ARIMA model
 * @param x Original time series used for fitting
 * @param n Length of original series
 * @param horizon Number of steps to forecast
 * @param forecast Output forecasted values [horizon]
 * @param std_errors Output standard errors [horizon] (can be NULL)
 * @return NIMCP_TS_OK on success
 *
 * The forecast uncertainty increases with horizon.
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_arima_predict(
    const nimcp_arima_model_t* model,
    const float* x,
    uint32_t n,
    uint32_t horizon,
    float* forecast,
    float* std_errors
);

/**
 * @brief Get residuals from fitted ARIMA model
 *
 * @param model Fitted ARIMA model
 * @param x Original time series
 * @param n Length of original series
 * @param residuals Output residuals [n] (can be shorter by d+max(p,q))
 * @param n_residuals Output: actual number of residuals computed
 * @return NIMCP_TS_OK on success
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_arima_residuals(
    const nimcp_arima_model_t* model,
    const float* x,
    uint32_t n,
    float* residuals,
    uint32_t* n_residuals
);

/**
 * @brief Get AIC and BIC for model selection
 *
 * @param model Fitted ARIMA model
 * @param aic Output AIC value
 * @param bic Output BIC value
 * @param aicc Output AICc value (corrected for small samples)
 * @return NIMCP_TS_OK on success
 *
 * SELECTION: Choose model with lowest AIC/BIC among candidates.
 *   AIC = -2*log(L) + 2*k (prefers simpler models)
 *   BIC = -2*log(L) + k*log(n) (more penalization for complexity)
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_arima_aic_bic(
    const nimcp_arima_model_t* model,
    float* aic,
    float* bic,
    float* aicc
);

/**
 * @brief Automatic ARIMA order selection
 *
 * @param x Time series data
 * @param n Number of observations
 * @param max_p Maximum AR order to consider
 * @param max_d Maximum differencing order
 * @param max_q Maximum MA order
 * @param criterion Selection criterion: 'a' = AIC, 'b' = BIC, 'c' = AICc
 * @param model Output: best fitted model (caller must free)
 * @return NIMCP_TS_OK on success
 *
 * METHOD: Grid search over (p,d,q) combinations with automatic
 * differencing detection via unit root tests.
 *
 * NOTE: Can be slow for large max orders. Typical max_p, max_q = 5.
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_arima_auto(
    const float* x,
    uint32_t n,
    uint32_t max_p,
    uint32_t max_d,
    uint32_t max_q,
    char criterion,
    nimcp_arima_model_t** model
);

//=============================================================================
// Stationarity Testing
//=============================================================================

/**
 * @brief Augmented Dickey-Fuller test for unit root
 *
 * @param x Time series data
 * @param n Number of observations
 * @param max_lags Maximum lags (0 = automatic selection)
 * @param regression Regression type: 'c' = constant, 'ct' = constant+trend, 'n' = none
 * @param result Output test result
 * @return NIMCP_TS_OK on success
 *
 * HYPOTHESES:
 *   H0: Unit root present (non-stationary)
 *   H1: No unit root (stationary)
 *
 * DECISION: Reject H0 if statistic < critical value (stationary)
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_ts_adf_test(
    const float* x,
    uint32_t n,
    uint32_t max_lags,
    const char* regression,
    nimcp_stationarity_result_t* result
);

/**
 * @brief KPSS stationarity test
 *
 * @param x Time series data
 * @param n Number of observations
 * @param n_lags Number of lags for HAC estimator (0 = automatic)
 * @param trend Test for level stationarity ('c') or trend stationarity ('ct')
 * @param result Output test result
 * @return NIMCP_TS_OK on success
 *
 * HYPOTHESES (opposite of ADF):
 *   H0: Series is stationary
 *   H1: Series is non-stationary
 *
 * DECISION: Reject H0 if statistic > critical value (non-stationary)
 *
 * RECOMMENDATION: Use both ADF and KPSS together for robust conclusions.
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_ts_kpss_test(
    const float* x,
    uint32_t n,
    uint32_t n_lags,
    char trend,
    nimcp_stationarity_result_t* result
);

/**
 * @brief Phillips-Perron test for unit root
 *
 * @param x Time series data
 * @param n Number of observations
 * @param n_lags Number of lags for Newey-West estimator
 * @param regression Regression type: 'c', 'ct', or 'n'
 * @param result Output test result
 * @return NIMCP_TS_OK on success
 *
 * Similar to ADF but uses non-parametric correction for serial correlation.
 * More robust when serial correlation structure is unknown.
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_ts_pp_test(
    const float* x,
    uint32_t n,
    uint32_t n_lags,
    const char* regression,
    nimcp_stationarity_result_t* result
);

//=============================================================================
// Smoothing and Filtering
//=============================================================================

/**
 * @brief Simple moving average smoothing
 *
 * @param x Input time series
 * @param n Number of observations
 * @param window_size Window size (must be odd for centered MA)
 * @param centered Use centered moving average
 * @param out Output smoothed series [n] (can be same as x for in-place)
 * @return NIMCP_TS_OK on success
 *
 * Boundary handling: Uses available data at edges.
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_ts_moving_average(
    const float* x,
    uint32_t n,
    uint32_t window_size,
    bool centered,
    float* out
);

/**
 * @brief Exponential smoothing (simple exponential smoothing)
 *
 * @param x Input time series
 * @param n Number of observations
 * @param alpha Smoothing parameter in (0, 1)
 * @param out Output smoothed series [n]
 * @return NIMCP_TS_OK on success
 *
 * FORMULA: s_t = alpha * x_t + (1 - alpha) * s_{t-1}
 *
 * INTERPRETATION:
 *   alpha close to 1: More responsive to recent changes
 *   alpha close to 0: More smoothing (slower response)
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_ts_exponential_smooth(
    const float* x,
    uint32_t n,
    float alpha,
    float* out
);

/**
 * @brief Holt-Winters exponential smoothing (seasonal)
 *
 * @param x Input time series
 * @param n Number of observations
 * @param params Holt-Winters parameters (can have pre-set alpha/beta/gamma or NULL for auto)
 * @param out Output smoothed series [n]
 * @return NIMCP_TS_OK on success
 *
 * Handles trend and seasonality. Requires n >= 2*period.
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_ts_holt_winters(
    const float* x,
    uint32_t n,
    nimcp_hw_params_t* params,
    float* out
);

/**
 * @brief Create Holt-Winters parameters structure
 *
 * @param period Seasonal period
 * @param additive true for additive seasonality, false for multiplicative
 * @return Allocated parameters (caller must free with nimcp_hw_free)
 */
NIMCP_EXPORT nimcp_hw_params_t* nimcp_hw_create(uint32_t period, bool additive);

/**
 * @brief Free Holt-Winters parameters
 */
NIMCP_EXPORT void nimcp_hw_free(nimcp_hw_params_t* params);

/**
 * @brief Kalman filter for state estimation
 *
 * @param observations Input observations [n x obs_dim]
 * @param n Number of observations
 * @param state Kalman filter state (must be initialized)
 * @param filtered_states Output filtered state estimates [n x state_dim]
 * @param filtered_covs Output filtered covariances [n x state_dim x state_dim] (can be NULL)
 * @return NIMCP_TS_OK on success
 *
 * USAGE: Initialize state with transition model matrices before calling.
 * See nimcp_kalman_create() and nimcp_kalman_init_*() helpers.
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_ts_kalman_filter(
    const float* observations,
    uint32_t n,
    nimcp_kalman_state_t* state,
    float* filtered_states,
    float* filtered_covs
);

/**
 * @brief Create Kalman filter state structure
 *
 * @param state_dim State dimension
 * @param obs_dim Observation dimension
 * @return Allocated Kalman state (caller must free with nimcp_kalman_free)
 */
NIMCP_EXPORT nimcp_kalman_state_t* nimcp_kalman_create(uint32_t state_dim, uint32_t obs_dim);

/**
 * @brief Initialize Kalman filter for local level model (random walk)
 *
 * @param state Kalman state structure
 * @param initial_level Initial state estimate
 * @param obs_variance Observation noise variance
 * @param state_variance State evolution variance
 * @return NIMCP_TS_OK on success
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_kalman_init_local_level(
    nimcp_kalman_state_t* state,
    float initial_level,
    float obs_variance,
    float state_variance
);

/**
 * @brief Initialize Kalman filter for local linear trend model
 *
 * @param state Kalman state structure (must have state_dim >= 2)
 * @param initial_level Initial level estimate
 * @param initial_trend Initial trend estimate
 * @param obs_variance Observation noise variance
 * @param level_variance Level evolution variance
 * @param trend_variance Trend evolution variance
 * @return NIMCP_TS_OK on success
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_kalman_init_local_trend(
    nimcp_kalman_state_t* state,
    float initial_level,
    float initial_trend,
    float obs_variance,
    float level_variance,
    float trend_variance
);

/**
 * @brief Free Kalman filter state
 */
NIMCP_EXPORT void nimcp_kalman_free(nimcp_kalman_state_t* state);

/**
 * @brief Savitzky-Golay smoothing filter
 *
 * @param x Input time series
 * @param n Number of observations
 * @param window_size Window size (must be odd)
 * @param poly_order Polynomial order (must be < window_size)
 * @param deriv Derivative order (0 = smoothing, 1 = first derivative, etc.)
 * @param out Output filtered series [n]
 * @return NIMCP_TS_OK on success
 *
 * ADVANTAGES:
 *   - Preserves signal features better than moving average
 *   - Can compute smoothed derivatives
 *   - Optimal for polynomial signals
 *
 * TYPICAL: window_size=11, poly_order=3 for general smoothing
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_ts_savitzky_golay(
    const float* x,
    uint32_t n,
    uint32_t window_size,
    uint32_t poly_order,
    uint32_t deriv,
    float* out
);

//=============================================================================
// Causality Analysis
//=============================================================================

/**
 * @brief Granger causality test
 *
 * @param x Potential cause time series
 * @param y Effect time series (what we're predicting)
 * @param n Number of observations (must be same for both)
 * @param max_lag Maximum lag to include
 * @param result Output test result
 * @return NIMCP_TS_OK on success
 *
 * TESTS: Whether past values of X help predict Y beyond past values of Y alone.
 *
 * HYPOTHESES:
 *   H0: X does not Granger-cause Y
 *   H1: X Granger-causes Y
 *
 * REQUIREMENTS:
 *   - Both series should be stationary
 *   - n > 3 * max_lag + 10
 *
 * NOTE: Granger causality is statistical, not true causation.
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_ts_granger_causality(
    const float* x,
    const float* y,
    uint32_t n,
    uint32_t max_lag,
    nimcp_granger_result_t* result
);

/**
 * @brief Compute transfer entropy from X to Y
 *
 * @param x Source time series
 * @param y Target time series
 * @param n Number of observations
 * @param k History length (embedding dimension)
 * @param delay Delay parameter (typically 1)
 * @param n_bins Number of bins for discretization (0 = auto)
 * @return Transfer entropy in bits, or NAN on error
 *
 * FORMULA: T(X->Y) = H(Y_t | Y_past) - H(Y_t | Y_past, X_past)
 *
 * INTERPRETATION: How much knowing X's past reduces uncertainty about Y's future
 * beyond what Y's past already tells us. Information-theoretic causality measure.
 *
 * ADVANTAGES over Granger:
 *   - Detects nonlinear causal relationships
 *   - Model-free
 */
NIMCP_EXPORT float nimcp_ts_transfer_entropy(
    const float* x,
    const float* y,
    uint32_t n,
    uint32_t k,
    uint32_t delay,
    uint32_t n_bins
);

/**
 * @brief Convergent Cross Mapping (CCM) for nonlinear causality
 *
 * @param x First time series
 * @param y Second time series
 * @param n Number of observations
 * @param E Embedding dimension
 * @param tau Time delay for embedding
 * @param lib_sizes Library sizes to test [n_lib_sizes]
 * @param n_lib_sizes Number of library sizes
 * @param rho_xy Output: correlation of X->Y prediction [n_lib_sizes]
 * @param rho_yx Output: correlation of Y->X prediction [n_lib_sizes]
 * @return NIMCP_TS_OK on success
 *
 * METHOD: Tests whether manifold of Y can predict X (indicates X causes Y).
 * Based on Sugihara et al. (2012) Science paper.
 *
 * INTERPRETATION:
 *   If rho_xy increases with library size: X causally influences Y
 *   If rho_yx increases with library size: Y causally influences X
 *
 * REQUIREMENTS:
 *   - Both series must be from deterministic dynamical system
 *   - n > 100 typically needed for robust results
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_ts_convergent_cross_mapping(
    const float* x,
    const float* y,
    uint32_t n,
    uint32_t E,
    uint32_t tau,
    const uint32_t* lib_sizes,
    uint32_t n_lib_sizes,
    float* rho_xy,
    float* rho_yx
);

//=============================================================================
// Change Point Detection
//=============================================================================

/**
 * @brief CUSUM (Cumulative Sum) change detection
 *
 * @param x Time series data
 * @param n Number of observations
 * @param threshold Detection threshold (0 = automatic)
 * @param drift Allowable drift (k parameter, typically 0.5*sigma)
 * @param result Output change point result (caller must free)
 * @return NIMCP_TS_OK on success
 *
 * DETECTS: Shifts in the mean level of the series.
 *
 * METHOD: Tracks cumulative deviations from target mean.
 * Signals when CUSUM exceeds threshold.
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_ts_cusum(
    const float* x,
    uint32_t n,
    float threshold,
    float drift,
    nimcp_changepoint_result_t* result
);

/**
 * @brief Pettitt test for single change point
 *
 * @param x Time series data
 * @param n Number of observations
 * @param result Output change point result
 * @return NIMCP_TS_OK on success
 *
 * DETECTS: Location of single most significant change point.
 *
 * METHOD: Non-parametric rank-based test.
 * Returns the point k that maximizes |U_k|.
 *
 * OUTPUT: result->n_points = 1, result->locations[0] = change point index
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_ts_pettitt_test(
    const float* x,
    uint32_t n,
    nimcp_changepoint_result_t* result
);

/**
 * @brief Binary segmentation for multiple change points
 *
 * @param x Time series data
 * @param n Number of observations
 * @param min_segment_length Minimum segment length between change points
 * @param threshold Significance threshold
 * @param max_changepoints Maximum number of change points (0 = unlimited)
 * @param result Output change point result (caller must free)
 * @return NIMCP_TS_OK on success
 *
 * METHOD: Recursively applies single change point detection to segments.
 * Stops when no significant change points remain or segment too small.
 *
 * COST: O(n log n) for balanced splits, O(n^2) worst case.
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_ts_binseg(
    const float* x,
    uint32_t n,
    uint32_t min_segment_length,
    float threshold,
    uint32_t max_changepoints,
    nimcp_changepoint_result_t* result
);

/**
 * @brief Free change point result
 */
NIMCP_EXPORT void nimcp_changepoint_free(nimcp_changepoint_result_t* result);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Apply differencing to time series
 *
 * @param x Input time series
 * @param n Number of observations
 * @param d Differencing order (1, 2, ...)
 * @param out Output differenced series [n - d]
 * @return NIMCP_TS_OK on success
 *
 * FORMULA: diff(x, d=1) = x[t] - x[t-1]
 *          diff(x, d=2) = diff(diff(x, d=1))
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_ts_difference(
    const float* x,
    uint32_t n,
    uint32_t d,
    float* out
);

/**
 * @brief Apply seasonal differencing
 *
 * @param x Input time series
 * @param n Number of observations
 * @param period Seasonal period
 * @param out Output differenced series [n - period]
 * @return NIMCP_TS_OK on success
 *
 * FORMULA: x[t] - x[t - period]
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_ts_seasonal_difference(
    const float* x,
    uint32_t n,
    uint32_t period,
    float* out
);

/**
 * @brief Detrend time series
 *
 * @param x Input time series
 * @param n Number of observations
 * @param method Detrending method
 * @param poly_order Polynomial order (for DETREND_POLYNOMIAL)
 * @param out Output detrended series [n]
 * @return NIMCP_TS_OK on success
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_ts_detrend(
    const float* x,
    uint32_t n,
    nimcp_ts_detrend_t method,
    uint32_t poly_order,
    float* out
);

/**
 * @brief Interpolate irregularly sampled time series to regular grid
 *
 * @param times Input time stamps [n]
 * @param values Input values [n]
 * @param n Number of input points
 * @param regular_times Output regular time grid [n_out]
 * @param n_out Number of output points
 * @param method Interpolation method: 'l' = linear, 'c' = cubic, 'n' = nearest
 * @param out Output interpolated values [n_out]
 * @return NIMCP_TS_OK on success
 *
 * HANDLING: NaN values in output indicate extrapolation beyond input range.
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_ts_interpolate(
    const float* times,
    const float* values,
    uint32_t n,
    const float* regular_times,
    uint32_t n_out,
    char method,
    float* out
);

/**
 * @brief Fill missing values (NaN) in time series
 *
 * @param x Time series with NaN values [n]
 * @param n Number of observations
 * @param method Fill method: 'f' = forward fill, 'b' = backward fill,
 *               'l' = linear interpolation, 'm' = mean fill
 * @param out Output filled series [n]
 * @return NIMCP_TS_OK on success
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_ts_fillna(
    const float* x,
    uint32_t n,
    char method,
    float* out
);

/**
 * @brief Count NaN values in time series
 *
 * @param x Time series
 * @param n Number of observations
 * @return Number of NaN values
 */
NIMCP_EXPORT uint32_t nimcp_ts_count_nan(const float* x, uint32_t n);

/**
 * @brief Apply window function to data
 *
 * @param n Window length
 * @param window_type Window function type
 * @param param Window parameter (beta for Kaiser, sigma for Gaussian)
 * @param out Output window coefficients [n]
 * @return NIMCP_TS_OK on success
 */
NIMCP_EXPORT nimcp_ts_result_t nimcp_ts_window_function(
    uint32_t n,
    nimcp_ts_window_t window_type,
    float param,
    float* out
);

/**
 * @brief Get error message for result code
 * @param result Result code
 * @return Human-readable error message
 */
NIMCP_EXPORT const char* nimcp_ts_error_string(nimcp_ts_result_t result);

/**
 * @brief Free ACF result arrays
 */
NIMCP_EXPORT void nimcp_acf_free(nimcp_acf_result_t* result);

/**
 * @brief Free PACF result arrays
 */
NIMCP_EXPORT void nimcp_pacf_free(nimcp_pacf_result_t* result);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_TIMESERIES_H
