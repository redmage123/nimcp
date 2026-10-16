/**
 * @file nimcp_basilar_membrane.h
 * @brief Biologically-accurate basilar membrane model with gammatone filterbank
 *
 * WHAT: Gammatone/gammachirp filterbank simulating cochlear mechanics
 * WHY:  Enable frequency analysis matching biological auditory periphery
 * HOW:  ERB-spaced filters with active amplification support
 *
 * BIOLOGICAL BASIS:
 * - Basilar membrane traveling wave creates place-frequency mapping
 * - Base = high frequency, apex = low frequency (tonotopy)
 * - Gammatone filters approximate auditory filter shapes
 * - ERB (Equivalent Rectangular Bandwidth) spacing matches psychophysics
 *
 * SPECIES SUPPORT:
 * - Human: 20 Hz - 20 kHz (128 default channels)
 * - Dog: 67 Hz - 65 kHz (192 channels for ultrasonic)
 * - Bat: 1 kHz - 200 kHz (256 channels for echolocation)
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#ifndef NIMCP_BASILAR_MEMBRANE_H
#define NIMCP_BASILAR_MEMBRANE_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration Constants
//=============================================================================

/** Default number of frequency channels (human hearing) */
#define BM_DEFAULT_NUM_CHANNELS     128

/** Human frequency range */
#define BM_HUMAN_MIN_FREQ_HZ        20.0f
#define BM_HUMAN_MAX_FREQ_HZ        20000.0f

/** Dog frequency range */
#define BM_DOG_MIN_FREQ_HZ          67.0f
#define BM_DOG_MAX_FREQ_HZ          65000.0f
#define BM_DOG_DEFAULT_CHANNELS     192

/** Bat frequency range */
#define BM_BAT_MIN_FREQ_HZ          1000.0f
#define BM_BAT_MAX_FREQ_HZ          200000.0f
#define BM_BAT_DEFAULT_CHANNELS     256

/** Maximum supported sample rate */
#define BM_MAX_SAMPLE_RATE          500000  /* 500 kHz for bat ultrasonics */

/** Maximum number of channels */
#define BM_MAX_CHANNELS             512

/** Gammatone filter order (biological: 4) */
#define BM_GAMMATONE_ORDER          4

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Filter type selection
 *
 * BIOLOGICAL:
 * - Gammatone: Classic auditory filter, good for speech/music
 * - Gammachirp: Includes phase compensation, better psychophysical match
 */
typedef enum {
    BM_FILTER_GAMMATONE,            /**< Standard gammatone filters */
    BM_FILTER_GAMMACHIRP,           /**< Gammachirp with asymmetric tuning */
    BM_FILTER_ROEX                  /**< Rounded exponential (Patterson) */
} bm_filter_type_t;

/**
 * @brief Frequency spacing mode
 *
 * BIOLOGICAL: ERB scale matches critical band spacing of cochlea
 */
typedef enum {
    BM_SPACING_ERB,                 /**< Equivalent Rectangular Bandwidth */
    BM_SPACING_BARK,                /**< Bark scale */
    BM_SPACING_MEL,                 /**< Mel scale */
    BM_SPACING_LINEAR,              /**< Linear spacing */
    BM_SPACING_LOG                  /**< Logarithmic spacing */
} bm_spacing_type_t;

/**
 * @brief Hearing mode for species-specific parameters
 */
typedef enum {
    BM_MODE_HUMAN,                  /**< Human hearing (20 Hz - 20 kHz) */
    BM_MODE_DOG,                    /**< Dog hearing (67 Hz - 65 kHz) */
    BM_MODE_BAT,                    /**< Bat hearing (1 kHz - 200 kHz) */
    BM_MODE_HYBRID,                 /**< Combined mode for multispecies */
    BM_MODE_CUSTOM                  /**< Custom frequency range */
} bm_hearing_mode_t;

//=============================================================================
// Core Structures
//=============================================================================

/**
 * @brief Single gammatone filter state
 *
 * Each filter represents one "place" on the basilar membrane.
 * State variables maintain filter memory across processing frames.
 */
typedef struct {
    float center_freq_hz;           /**< Center frequency in Hz */
    float bandwidth_hz;             /**< Bandwidth (ERB at CF) */
    float gain;                     /**< Filter gain (linear) */

    /* Filter coefficients (4th order) */
    float a_coeffs[BM_GAMMATONE_ORDER];     /**< Feedback coefficients */
    float b_coeffs[BM_GAMMATONE_ORDER + 1]; /**< Feedforward coefficients */

    /* Filter state (4 poles, 2 states each for real/complex) */
    float state_real[BM_GAMMATONE_ORDER];   /**< Real state */
    float state_imag[BM_GAMMATONE_ORDER];   /**< Imaginary state */

    /* Output state */
    float envelope;                 /**< Current envelope output */
    float instantaneous_freq;       /**< Instantaneous frequency (Hz) */
    float phase;                    /**< Current phase (radians) */

} bm_filter_t;

/**
 * @brief Basilar membrane configuration
 */
typedef struct {
    /* Frequency range */
    float min_freq_hz;              /**< Minimum center frequency */
    float max_freq_hz;              /**< Maximum center frequency */
    uint32_t num_channels;          /**< Number of frequency channels */

    /* Filter parameters */
    bm_filter_type_t filter_type;   /**< Gammatone/gammachirp/roex */
    bm_spacing_type_t spacing;      /**< Frequency spacing mode */
    uint32_t filter_order;          /**< Filter order (default: 4) */

    /* Hearing mode */
    bm_hearing_mode_t mode;         /**< Species/hearing mode */

    /* Sample rate */
    uint32_t sample_rate;           /**< Audio sample rate (Hz) */

    /* Active processing options */
    bool enable_envelope;           /**< Compute envelope output */
    bool enable_phase;              /**< Track phase information */
    bool enable_fine_structure;     /**< Preserve temporal fine structure */

    /* Normalization */
    bool normalize_output;          /**< Normalize channel outputs */
    float reference_db;             /**< Reference level for dB conversion */

} bm_config_t;

/**
 * @brief Basilar membrane output structure
 *
 * Contains processed output from all frequency channels
 */
typedef struct {
    float* channel_output;          /**< Raw filter outputs [num_channels] */
    float* envelope;                /**< Envelope outputs [num_channels] */
    float* fine_structure;          /**< Fine structure [num_channels] */
    float* phase;                   /**< Phase values [num_channels] */

    uint32_t num_channels;          /**< Number of channels */
    uint32_t num_samples;           /**< Samples in this frame */

    /* Aggregate measures */
    float total_energy;             /**< Sum of all channel energies */
    float peak_frequency_hz;        /**< Frequency with maximum energy */
    uint32_t peak_channel;          /**< Channel index with max energy */

} bm_output_t;

/**
 * @brief Basilar membrane instance (opaque)
 */
typedef struct basilar_membrane basilar_membrane_t;

/**
 * @brief Basilar membrane statistics
 */
typedef struct {
    uint64_t samples_processed;     /**< Total samples processed */
    uint64_t frames_processed;      /**< Total frames processed */
    float avg_processing_time_us;   /**< Average processing time (microseconds) */
    float peak_processing_time_us;  /**< Peak processing time */
    float current_energy_db;        /**< Current signal energy (dB) */
} bm_stats_t;

//=============================================================================
// Configuration Helpers
//=============================================================================

/**
 * @brief Get default configuration for hearing mode
 *
 * WHAT: Initialize config with biologically-appropriate defaults
 * WHY:  Simplify setup for common use cases
 * HOW:  Set frequency range and channel count per species
 *
 * @param mode Hearing mode (human/dog/bat/hybrid)
 * @return Default configuration for specified mode
 */
bm_config_t bm_config_default(bm_hearing_mode_t mode);

/**
 * @brief Validate configuration
 *
 * @param config Configuration to validate
 * @return NIMCP_SUCCESS if valid, error code otherwise
 */
nimcp_error_t bm_config_validate(const bm_config_t* config);

//=============================================================================
// Core API
//=============================================================================

/**
 * @brief Create basilar membrane instance
 *
 * WHAT: Allocate and initialize filterbank
 * WHY:  Prepare for frequency analysis of audio
 * HOW:  Compute filter coefficients for each channel
 *
 * BIOLOGICAL: Models the traveling wave decomposition along cochlea
 *
 * @param config Configuration parameters
 * @return Basilar membrane instance or NULL on failure
 */
basilar_membrane_t* basilar_membrane_create(const bm_config_t* config);

/**
 * @brief Destroy basilar membrane instance
 *
 * @param bm Basilar membrane to destroy (can be NULL)
 */
void basilar_membrane_destroy(basilar_membrane_t* bm);

/**
 * @brief Process audio samples through filterbank
 *
 * WHAT: Filter audio through all frequency channels
 * WHY:  Decompose signal into tonotopic representation
 * HOW:  Apply gammatone filtering with envelope extraction
 *
 * BIOLOGICAL: Simulates mechanical vibration of basilar membrane
 *
 * @param bm Basilar membrane instance
 * @param audio_in Input audio samples (mono, float32)
 * @param num_samples Number of input samples
 * @param output Output structure (pre-allocated arrays)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t basilar_membrane_process(
    basilar_membrane_t* bm,
    const float* audio_in,
    uint32_t num_samples,
    bm_output_t* output
);

/**
 * @brief Process single sample (sample-by-sample mode)
 *
 * WHAT: Process one sample for real-time/low-latency use
 * WHY:  Enable streaming audio processing
 * HOW:  Update all filter states for single sample
 *
 * @param bm Basilar membrane instance
 * @param sample Input sample
 * @param channel_outputs Output array [num_channels]
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t basilar_membrane_process_sample(
    basilar_membrane_t* bm,
    float sample,
    float* channel_outputs
);

/**
 * @brief Reset all filter states
 *
 * @param bm Basilar membrane instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t basilar_membrane_reset(basilar_membrane_t* bm);

//=============================================================================
// Output Allocation
//=============================================================================

/**
 * @brief Create output structure with allocated arrays
 *
 * @param bm Basilar membrane instance
 * @param max_samples Maximum samples per processing call
 * @return Output structure or NULL on failure
 */
bm_output_t* bm_output_create(basilar_membrane_t* bm, uint32_t max_samples);

/**
 * @brief Destroy output structure
 *
 * @param output Output structure to destroy
 */
void bm_output_destroy(bm_output_t* output);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get number of frequency channels
 *
 * @param bm Basilar membrane instance
 * @return Number of channels, or 0 on error
 */
uint32_t basilar_membrane_get_num_channels(const basilar_membrane_t* bm);

/**
 * @brief Get center frequency for channel
 *
 * @param bm Basilar membrane instance
 * @param channel Channel index (0-based)
 * @return Center frequency in Hz, or -1 on error
 */
float basilar_membrane_get_center_freq(
    const basilar_membrane_t* bm,
    uint32_t channel
);

/**
 * @brief Get bandwidth for channel
 *
 * @param bm Basilar membrane instance
 * @param channel Channel index
 * @return Bandwidth in Hz, or -1 on error
 */
float basilar_membrane_get_bandwidth(
    const basilar_membrane_t* bm,
    uint32_t channel
);

/**
 * @brief Get array of all center frequencies
 *
 * @param bm Basilar membrane instance
 * @param freqs Output array [num_channels] (must be pre-allocated)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t basilar_membrane_get_all_center_freqs(
    const basilar_membrane_t* bm,
    float* freqs
);

/**
 * @brief Find channel index for given frequency
 *
 * @param bm Basilar membrane instance
 * @param freq_hz Frequency to find (Hz)
 * @return Channel index, or -1 if out of range
 */
int32_t basilar_membrane_freq_to_channel(
    const basilar_membrane_t* bm,
    float freq_hz
);

/**
 * @brief Get statistics
 *
 * @param bm Basilar membrane instance
 * @param stats Output statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t basilar_membrane_get_stats(
    const basilar_membrane_t* bm,
    bm_stats_t* stats
);

//=============================================================================
// Gain and Modulation
//=============================================================================

/**
 * @brief Set channel gain (OHC amplification simulation)
 *
 * WHAT: Modify gain for specific frequency channel
 * WHY:  Simulate outer hair cell amplification
 * HOW:  Multiply filter output by gain factor
 *
 * BIOLOGICAL: OHCs provide 40-60 dB active amplification
 *
 * @param bm Basilar membrane instance
 * @param channel Channel index
 * @param gain_linear Linear gain factor (1.0 = unity)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t basilar_membrane_set_channel_gain(
    basilar_membrane_t* bm,
    uint32_t channel,
    float gain_linear
);

/**
 * @brief Set all channel gains
 *
 * @param bm Basilar membrane instance
 * @param gains Array of gains [num_channels]
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t basilar_membrane_set_all_gains(
    basilar_membrane_t* bm,
    const float* gains
);

/**
 * @brief Apply frequency-dependent gain curve
 *
 * WHAT: Set gains based on frequency weighting
 * WHY:  Model sensitivity curves or attention effects
 * HOW:  Interpolate gain values across frequencies
 *
 * @param bm Basilar membrane instance
 * @param freq_points Frequency points (Hz)
 * @param gain_points Gain values at each frequency
 * @param num_points Number of points
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t basilar_membrane_apply_gain_curve(
    basilar_membrane_t* bm,
    const float* freq_points,
    const float* gain_points,
    uint32_t num_points
);

//=============================================================================
// ERB Utility Functions
//=============================================================================

/**
 * @brief Compute ERB (Equivalent Rectangular Bandwidth) at frequency
 *
 * Formula: ERB(f) = 24.7 * (4.37 * f/1000 + 1)
 *
 * @param freq_hz Frequency in Hz
 * @return ERB in Hz
 */
float bm_erb_at_freq(float freq_hz);

/**
 * @brief Convert frequency to ERB scale (ERB number)
 *
 * Formula: ERB_number = 21.4 * log10(4.37 * f/1000 + 1)
 *
 * @param freq_hz Frequency in Hz
 * @return ERB number
 */
float bm_freq_to_erb(float freq_hz);

/**
 * @brief Convert ERB scale to frequency
 *
 * @param erb ERB number
 * @return Frequency in Hz
 */
float bm_erb_to_freq(float erb);

/**
 * @brief Compute center frequencies with ERB spacing
 *
 * @param min_freq Minimum frequency (Hz)
 * @param max_freq Maximum frequency (Hz)
 * @param num_channels Number of channels
 * @param center_freqs Output array [num_channels]
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t bm_compute_erb_spaced_freqs(
    float min_freq,
    float max_freq,
    uint32_t num_channels,
    float* center_freqs
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BASILAR_MEMBRANE_H */
