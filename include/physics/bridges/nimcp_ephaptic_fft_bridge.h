//=============================================================================
// nimcp_ephaptic_fft_bridge.h - Ephaptic FFT Integration Bridge
//=============================================================================
/**
 * @file nimcp_ephaptic_fft_bridge.h
 * @brief FFT-based spectral analysis for Ephaptic coupling LFP signals
 *
 * WHAT: Provides proper FFT-based band power computation for LFP signals
 *       from the ephaptic coupling module, replacing hardcoded estimates.
 *
 * WHY:  The ephaptic module generates LFP signals but previously used
 *       hardcoded band power estimates. Proper FFT analysis enables:
 *       - Accurate measurement of oscillatory power in frequency bands
 *       - Detection of dominant frequencies in neural activity
 *       - Cross-frequency coupling analysis
 *       - Spectral coherence computation
 *
 * HOW:  - Maintains circular buffer of LFP time series
 *       - Pre-allocates FFT plan for efficient repeated computation
 *       - Provides both instant and time-averaged band power
 *       - Uses Hann windowing to reduce spectral leakage
 *
 * BIOLOGICAL: LFP reflects summed synaptic activity and is organized into
 * frequency bands with distinct functional roles:
 *   - Delta (1-4 Hz): Deep sleep, slow-wave activity
 *   - Theta (4-8 Hz): Memory encoding, navigation
 *   - Alpha (8-13 Hz): Idle/inhibition, attention gating
 *   - Beta (13-30 Hz): Motor planning, working memory
 *   - Gamma (30-100 Hz): Feature binding, consciousness
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_EPHAPTIC_FFT_BRIDGE_H
#define NIMCP_EPHAPTIC_FFT_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"
#include "physics/ephaptic/nimcp_ephaptic.h"
#include "utils/spectral/nimcp_fft.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Default FFT size (power of 2) - 1024 samples = ~1s at 1kHz */
#define EPHAPTIC_FFT_DEFAULT_SIZE           1024

/** Default LFP sampling rate (Hz) - typical for neural recordings */
#define EPHAPTIC_FFT_DEFAULT_SAMPLE_RATE    1000.0f

/** Default window function for LFP analysis */
#define EPHAPTIC_FFT_DEFAULT_WINDOW         FFT_WINDOW_HANN

/** Minimum FFT size supported */
#define EPHAPTIC_FFT_MIN_SIZE               64

/** Maximum FFT size supported */
#define EPHAPTIC_FFT_MAX_SIZE               65536

/** Number of standard brain wave bands */
#define EPHAPTIC_FFT_NUM_BANDS              5

//=============================================================================
// Band Definitions (Hz)
//=============================================================================

/** Delta band: slow oscillations, deep sleep */
#define EPHAPTIC_BAND_DELTA_LOW     1.0f
#define EPHAPTIC_BAND_DELTA_HIGH    4.0f

/** Theta band: memory, navigation */
#define EPHAPTIC_BAND_THETA_LOW     4.0f
#define EPHAPTIC_BAND_THETA_HIGH    8.0f

/** Alpha band: idle/inhibition */
#define EPHAPTIC_BAND_ALPHA_LOW     8.0f
#define EPHAPTIC_BAND_ALPHA_HIGH    13.0f

/** Beta band: motor, working memory */
#define EPHAPTIC_BAND_BETA_LOW      13.0f
#define EPHAPTIC_BAND_BETA_HIGH     30.0f

/** Gamma band: binding, consciousness */
#define EPHAPTIC_BAND_GAMMA_LOW     30.0f
#define EPHAPTIC_BAND_GAMMA_HIGH    100.0f

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief FFT bridge configuration
 */
typedef struct {
    /** FFT size - must be power of 2 */
    uint32_t fft_size;

    /** LFP sampling rate (Hz) */
    float sampling_rate;

    /** Window function for spectral analysis */
    fft_window_t window;

    /** Enable time-averaged band power (exponential smoothing) */
    bool enable_averaging;

    /** Averaging time constant (seconds) - for exponential smoothing */
    float averaging_tau;

    /** Custom band boundaries (NULL = use defaults) */
    float* band_boundaries;  /**< Array of 6 floats: [d_lo, d_hi, t_lo, t_hi, a_lo, ...] */

    /** Enable spectral coherence computation */
    bool enable_coherence;
} ephaptic_fft_config_t;

/**
 * @brief Extended band power result with FFT-derived metrics
 */
typedef struct {
    /** Power in each frequency band (delta, theta, alpha, beta, gamma) */
    float band_power[EPHAPTIC_FFT_NUM_BANDS];

    /** Relative power (normalized to total power) */
    float band_power_relative[EPHAPTIC_FFT_NUM_BANDS];

    /** Total power across all frequencies */
    float total_power;

    /** Dominant frequency (Hz) */
    float dominant_frequency;

    /** Power at dominant frequency */
    float peak_power;

    /** Median frequency (Hz) - frequency below which 50% of power lies */
    float median_frequency;

    /** Spectral edge frequency (Hz) - frequency below which 95% of power lies */
    float spectral_edge_95;

    /** Spectral entropy - measure of spectral flatness */
    float spectral_entropy;

    /** Band power ratios for common clinical metrics */
    float theta_alpha_ratio;    /**< Theta/Alpha - attention metric */
    float theta_beta_ratio;     /**< Theta/Beta - arousal metric */
    float delta_alpha_ratio;    /**< Delta/Alpha - drowsiness metric */

    /** Timestamp of this measurement */
    float timestamp_ms;

    /** Is the FFT buffer fully populated */
    bool buffer_full;
} ephaptic_fft_result_t;

/**
 * @brief Opaque FFT bridge structure
 */
typedef struct ephaptic_fft_bridge_struct ephaptic_fft_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default FFT bridge configuration
 *
 * WHAT: Returns sensible defaults for LFP spectral analysis
 * WHY:  Provide biological plausible starting parameters
 * HOW:  Static defaults based on neuroscience literature
 *
 * @param config    Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_fft_default_config(ephaptic_fft_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create FFT bridge for an ephaptic system
 *
 * WHAT: Allocates FFT infrastructure for LFP spectral analysis
 * WHY:  Enable proper frequency-domain analysis of LFP signals
 * HOW:  Creates FFT plan, allocates circular buffer for time series
 *
 * @param config    Configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
NIMCP_EXPORT ephaptic_fft_bridge_t* ephaptic_fft_bridge_create(
    const ephaptic_fft_config_t* config
);

/**
 * @brief Destroy FFT bridge and free resources
 *
 * @param bridge    Bridge to destroy
 */
NIMCP_EXPORT void ephaptic_fft_bridge_destroy(ephaptic_fft_bridge_t* bridge);

/**
 * @brief Reset FFT bridge state (clear buffer, reset averages)
 *
 * @param bridge    Bridge to reset
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_fft_bridge_reset(ephaptic_fft_bridge_t* bridge);

//=============================================================================
// LFP Sample Collection API
//=============================================================================

/**
 * @brief Add a single LFP sample to the buffer
 *
 * WHAT: Appends LFP value to circular buffer for FFT
 * WHY:  Build up time series for spectral analysis
 * HOW:  Circular buffer with automatic wraparound
 *
 * @param bridge    FFT bridge
 * @param lfp_value LFP amplitude (mV)
 * @param timestamp Current time (ms)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_fft_add_sample(
    ephaptic_fft_bridge_t* bridge,
    float lfp_value,
    float timestamp
);

/**
 * @brief Add LFP samples from ephaptic system LFP computation
 *
 * WHAT: Convenience function to add LFP from compute_lfp result
 * WHY:  Integrate seamlessly with ephaptic module
 * HOW:  Extracts amplitude from nimcp_lfp_result_t
 *
 * @param bridge    FFT bridge
 * @param lfp       LFP result from nimcp_ephaptic_compute_lfp
 * @param timestamp Current time (ms)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_fft_add_lfp_result(
    ephaptic_fft_bridge_t* bridge,
    const nimcp_lfp_result_t* lfp,
    float timestamp
);

/**
 * @brief Check if buffer has enough samples for FFT
 *
 * @param bridge    FFT bridge
 * @return true if buffer is full (ready for FFT), false otherwise
 */
NIMCP_EXPORT bool ephaptic_fft_buffer_ready(const ephaptic_fft_bridge_t* bridge);

/**
 * @brief Get current buffer fill level (0.0 to 1.0)
 *
 * @param bridge    FFT bridge
 * @return Fill level as fraction of buffer size
 */
NIMCP_EXPORT float ephaptic_fft_buffer_level(const ephaptic_fft_bridge_t* bridge);

//=============================================================================
// Spectral Analysis API
//=============================================================================

/**
 * @brief Compute band power using FFT
 *
 * WHAT: Performs FFT on buffered LFP and computes band powers
 * WHY:  Replace hardcoded band power with proper spectral analysis
 * HOW:  Window → FFT → Power spectrum → Sum power in each band
 *
 * @param bridge    FFT bridge (must have full buffer)
 * @param result    Output band power result
 * @return 0 on success, -1 on error (e.g., buffer not full)
 */
NIMCP_EXPORT int ephaptic_fft_compute_band_power(
    ephaptic_fft_bridge_t* bridge,
    ephaptic_fft_result_t* result
);

/**
 * @brief Get time-averaged band power (if averaging enabled)
 *
 * WHAT: Returns exponentially smoothed band power
 * WHY:  Reduce noise in band power estimates
 * HOW:  Exponential moving average with configurable tau
 *
 * @param bridge    FFT bridge
 * @param result    Output averaged result
 * @return 0 on success, -1 if averaging not enabled or no data
 */
NIMCP_EXPORT int ephaptic_fft_get_averaged_power(
    const ephaptic_fft_bridge_t* bridge,
    ephaptic_fft_result_t* result
);

/**
 * @brief Get raw power spectrum
 *
 * WHAT: Returns the full power spectrum from last FFT
 * WHY:  Enable custom analysis beyond standard bands
 * HOW:  Copies internal power spectrum to output
 *
 * @param bridge    FFT bridge
 * @param power     Output power array (size = fft_size/2 + 1)
 * @param size      Size of output array
 * @return Number of values written, or -1 on error
 */
NIMCP_EXPORT int ephaptic_fft_get_power_spectrum(
    const ephaptic_fft_bridge_t* bridge,
    float* power,
    uint32_t size
);

/**
 * @brief Get frequency axis for power spectrum
 *
 * WHAT: Returns frequency values for each FFT bin
 * WHY:  Enable plotting and custom band analysis
 * HOW:  freq[k] = k * sampling_rate / fft_size
 *
 * @param bridge    FFT bridge
 * @param freq      Output frequency array (Hz)
 * @param size      Size of output array
 * @return Number of values written, or -1 on error
 */
NIMCP_EXPORT int ephaptic_fft_get_frequencies(
    const ephaptic_fft_bridge_t* bridge,
    float* freq,
    uint32_t size
);

//=============================================================================
// Integration with Ephaptic System
//=============================================================================

/**
 * @brief Compute LFP with FFT-based band power
 *
 * WHAT: Enhanced LFP computation using FFT for band power
 * WHY:  Replace hardcoded band power in nimcp_ephaptic_compute_lfp
 * HOW:  Computes LFP, adds to buffer, runs FFT if ready
 *
 * @param bridge    FFT bridge
 * @param system    Ephaptic system
 * @param position  Position for LFP computation (mm)
 * @param result    Output LFP result (with FFT-derived band power)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_fft_compute_lfp(
    ephaptic_fft_bridge_t* bridge,
    nimcp_ephaptic_system_t* system,
    const float position[3],
    nimcp_lfp_result_t* result
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get FFT configuration
 *
 * @param bridge    FFT bridge
 * @param config    Output configuration
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_fft_get_config(
    const ephaptic_fft_bridge_t* bridge,
    ephaptic_fft_config_t* config
);

/**
 * @brief Get number of frequency bins
 *
 * @param bridge    FFT bridge
 * @return Number of frequency bins (fft_size/2 + 1)
 */
NIMCP_EXPORT uint32_t ephaptic_fft_get_num_bins(const ephaptic_fft_bridge_t* bridge);

/**
 * @brief Get frequency resolution (Hz per bin)
 *
 * @param bridge    FFT bridge
 * @return Frequency resolution in Hz
 */
NIMCP_EXPORT float ephaptic_fft_get_frequency_resolution(
    const ephaptic_fft_bridge_t* bridge
);

/**
 * @brief Get Nyquist frequency
 *
 * @param bridge    FFT bridge
 * @return Nyquist frequency in Hz (sampling_rate / 2)
 */
NIMCP_EXPORT float ephaptic_fft_get_nyquist(const ephaptic_fft_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EPHAPTIC_FFT_BRIDGE_H */
