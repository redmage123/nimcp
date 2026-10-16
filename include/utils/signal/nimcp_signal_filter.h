//=============================================================================
// nimcp_signal_filter.h - Signal Filtering Utilities
//=============================================================================
/**
 * @file nimcp_signal_filter.h
 * @brief Band-pass, low-pass, and high-pass filtering for neural signals
 *
 * WHAT: Efficient FIR/IIR filtering using FFT-based convolution
 * WHY:  PAC detection requires frequency-specific signal extraction
 * HOW:  Windowed sinc filters with FFT acceleration for O(n log n) performance
 *
 * DESIGN PRINCIPLES:
 * - SRP: Single Responsibility - filtering only, no detection logic
 * - Modular: Separate filter design, application, and testing
 * - Performance: FFT-based convolution for large signals (>1024 samples)
 * - Memory-safe: All allocations via nimcp_memory API
 *
 * USAGE:
 *   signal_filter_config_t config = {
 *       .type = FILTER_BANDPASS,
 *       .low_freq = 4.0f,
 *       .high_freq = 8.0f,
 *       .sample_rate = 1000.0f,
 *       .order = 64
 *   };
 *   signal_filter_t* filter = signal_filter_create(&config);
 *   signal_filter_apply(filter, input, output, n);
 *   signal_filter_destroy(filter);
 */

#ifndef NIMCP_SIGNAL_FILTER_H
#define NIMCP_SIGNAL_FILTER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Filter types
 */
typedef enum {
    FILTER_LOWPASS,      ///< Low-pass filter (0 to cutoff)
    FILTER_HIGHPASS,     ///< High-pass filter (cutoff to Nyquist)
    FILTER_BANDPASS,     ///< Band-pass filter (low to high)
    FILTER_BANDSTOP      ///< Band-stop/notch filter (reject band)
} signal_filter_type_t;

/**
 * @brief Window functions for FIR filter design
 */
typedef enum {
    WINDOW_RECTANGULAR,  ///< Rectangular (box) window - sharpest transition, most ripple
    WINDOW_HAMMING,      ///< Hamming window - good general-purpose balance
    WINDOW_HANN,         ///< Hann window - smooth, good frequency resolution
    WINDOW_BLACKMAN      ///< Blackman window - widest transition, least ripple
} window_function_t;

/**
 * @brief Filter configuration
 */
typedef struct {
    signal_filter_type_t type;    ///< Filter type
    float low_freq;               ///< Low cutoff frequency (Hz) - for bandpass/bandstop
    float high_freq;              ///< High cutoff frequency (Hz) - for bandpass/bandstop
    float cutoff_freq;            ///< Cutoff frequency (Hz) - for lowpass/highpass
    float sample_rate;            ///< Sampling rate (Hz)
    uint32_t order;               ///< Filter order (number of taps - 1, must be even)
    window_function_t window;     ///< Window function
    bool use_fft_convolution;     ///< Use FFT for convolution (auto if n > 512)
} signal_filter_config_t;

/**
 * @brief Filter state (opaque handle)
 */
typedef struct signal_filter_t signal_filter_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default filter configuration
 * @return Default configuration (bandpass, Hamming window, order 64)
 */
signal_filter_config_t signal_filter_default_config(void);

/**
 * @brief Create bandpass filter configuration
 * @param low_freq Low cutoff frequency (Hz)
 * @param high_freq High cutoff frequency (Hz)
 * @param sample_rate Sampling rate (Hz)
 * @return Configured filter settings
 */
signal_filter_config_t signal_filter_bandpass_config(float low_freq, float high_freq, float sample_rate);

/**
 * @brief Create lowpass filter configuration
 * @param cutoff_freq Cutoff frequency (Hz)
 * @param sample_rate Sampling rate (Hz)
 * @return Configured filter settings
 */
signal_filter_config_t signal_filter_lowpass_config(float cutoff_freq, float sample_rate);

/**
 * @brief Create highpass filter configuration
 * @param cutoff_freq Cutoff frequency (Hz)
 * @param sample_rate Sampling rate (Hz)
 * @return Configured filter settings
 */
signal_filter_config_t signal_filter_highpass_config(float cutoff_freq, float sample_rate);

//=============================================================================
// Filter Lifecycle Functions
//=============================================================================

/**
 * @brief Create a signal filter
 * @param config Filter configuration
 * @return Filter handle or NULL on failure
 */
signal_filter_t* signal_filter_create(const signal_filter_config_t* config);

/**
 * @brief Destroy a signal filter
 * @param filter Filter to destroy
 */
void signal_filter_destroy(signal_filter_t* filter);

/**
 * @brief Reset filter state (clear history)
 * @param filter Filter to reset
 * @return True on success
 */
bool signal_filter_reset(signal_filter_t* filter);

//=============================================================================
// Filtering Functions
//=============================================================================

/**
 * @brief Apply filter to signal (in-place or out-of-place)
 * @param filter Filter to apply
 * @param input Input signal
 * @param output Output buffer (can be same as input for in-place)
 * @param n Number of samples
 * @return True on success
 */
bool signal_filter_apply(signal_filter_t* filter, const float* input, float* output, uint32_t n);

/**
 * @brief Apply filter and extract envelope (absolute value)
 * @param filter Filter to apply
 * @param input Input signal
 * @param envelope Output envelope (amplitude)
 * @param n Number of samples
 * @return True on success
 */
bool signal_filter_apply_envelope(signal_filter_t* filter, const float* input, float* envelope, uint32_t n);

/**
 * @brief Get filter coefficients (for testing/analysis)
 * @param filter Filter
 * @param coeffs Output buffer for coefficients
 * @param max_coeffs Maximum coefficients to copy
 * @param num_coeffs Actual number of coefficients
 * @return True on success
 */
bool signal_filter_get_coefficients(signal_filter_t* filter, float* coeffs, uint32_t max_coeffs, uint32_t* num_coeffs);

/**
 * @brief Get filter frequency response
 * @param filter Filter
 * @param frequencies Frequency points (Hz) to evaluate
 * @param response Output magnitude response (linear, not dB)
 * @param n Number of frequency points
 * @return True on success
 */
bool signal_filter_get_response(signal_filter_t* filter, const float* frequencies, float* response, uint32_t n);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Validate filter configuration
 * @param config Configuration to validate
 * @return True if valid
 */
bool signal_filter_validate_config(const signal_filter_config_t* config);

/**
 * @brief Estimate filter delay (group delay in samples)
 * @param filter Filter
 * @return Delay in samples (filter_order / 2 for linear phase FIR)
 */
uint32_t signal_filter_get_delay(signal_filter_t* filter);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SIGNAL_FILTER_H
