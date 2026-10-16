//=============================================================================
// nimcp_oscillation_detector.h - Neural Oscillation Detection
//=============================================================================

#ifndef NIMCP_OSCILLATION_DETECTOR_H
#define NIMCP_OSCILLATION_DETECTOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_oscillation_detector.h
 * @brief Detect oscillatory patterns in neural activity
 *
 * WHAT: Real-time detection of brain oscillations across frequency bands
 * WHY:  Oscillations coordinate neural processing and reflect cognitive states
 * HOW:  Band-pass filtering, power spectral density, phase locking analysis
 *
 * BIOLOGICAL BASIS:
 * - Delta (0-4Hz): Deep sleep, unconsciousness
 * - Theta (4-8Hz): Memory encoding, REM sleep, navigation
 * - Alpha (8-13Hz): Relaxed wakefulness, inhibitory control
 * - Beta (13-30Hz): Active thinking, motor control, anxiety
 * - Gamma (30-100Hz): Attention, binding, consciousness
 * - Cross-frequency coupling indicates information transfer
 *
 * ALGORITHMS:
 * - Band-pass filtering (Butterworth 4th order)
 * - Power spectral density (Welch's method with Hann window)
 * - Phase locking value (PLV) for synchronization
 * - Burst detection (amplitude threshold + duration)
 * - Phase-amplitude coupling (PAC) for cross-frequency interactions
 */

// ============================================================================
// CONSTANTS
// ============================================================================

#define OSC_NUM_BANDS 5                   // Standard brain oscillation bands
#define OSC_SAMPLE_RATE_HZ 1000.0f        // 1kHz sampling for neural data
#define OSC_WINDOW_SIZE 1024              // FFT window size (power of 2)
#define OSC_MIN_BURST_DURATION_MS 50.0f   // Minimum oscillation burst
#define OSC_BURST_THRESHOLD 2.0f          // Threshold in std devs above mean

// ============================================================================
// TYPES
// ============================================================================

/**
 * @brief Oscillation frequency bands
 */
typedef enum {
    OSC_BAND_DELTA = 0,   // 0-4 Hz
    OSC_BAND_THETA = 1,   // 4-8 Hz
    OSC_BAND_ALPHA = 2,   // 8-13 Hz
    OSC_BAND_BETA = 3,    // 13-30 Hz
    OSC_BAND_GAMMA = 4    // 30-100 Hz
} oscillation_band_t;

/**
 * @brief Band power result
 */
typedef struct {
    oscillation_band_t band;
    float power;                // Total power in band
    float relative_power;       // Power / total power
    float peak_frequency;       // Dominant frequency in band
    bool is_burst;              // Currently in burst
    float burst_duration_ms;    // Duration of current burst
} band_power_t;

/**
 * @brief Phase locking value result
 */
typedef struct {
    float plv;                  // Phase locking value [0, 1]
    float mean_phase_diff;      // Mean phase difference (radians)
    uint32_t num_samples;       // Samples in analysis
} phase_locking_t;

/**
 * @brief Cross-frequency coupling result
 */
typedef struct {
    oscillation_band_t phase_band;    // Lower frequency for phase
    oscillation_band_t amp_band;      // Higher frequency for amplitude
    float coupling_strength;          // PAC strength [0, 1]
    float preferred_phase;            // Phase of max amplitude (radians)
} cross_freq_coupling_t;

/**
 * @brief Oscillation detection result
 */
typedef struct {
    band_power_t bands[OSC_NUM_BANDS];      // Power in each band
    float total_power;                      // Total spectral power
    oscillation_band_t dominant_band;       // Highest power band
    uint32_t num_bursts;                    // Active burst count
    bool has_gamma;                         // Gamma activity present
    bool has_theta_gamma_coupling;          // Theta-gamma coupling detected
} oscillation_result_t;

/**
 * @brief Oscillation detector configuration
 */
typedef struct {
    float sample_rate_hz;               // Sampling rate
    uint32_t window_size;               // FFT window size
    float min_burst_duration_ms;        // Minimum burst duration
    float burst_threshold_std;          // Burst threshold (std devs)
    bool enable_burst_detection;        // Detect oscillation bursts
    bool enable_plv;                    // Compute phase locking
    bool enable_pac;                    // Detect cross-frequency coupling
    float overlap_fraction;             // Window overlap (0.0-1.0)
    bool use_phasor_detection;          // Use complex phasor methods (faster, more accurate)
} oscillation_detector_config_t;

/**
 * @brief Opaque oscillation detector handle
 */
typedef struct oscillation_detector oscillation_detector_t;

// ============================================================================
// API
// ============================================================================

/**
 * @brief Create oscillation detector with configuration
 *
 * WHAT: Initialize oscillation detection system
 * WHY:  Set up filters and spectral analysis engine
 * HOW:  Allocate buffers, initialize band-pass filters
 *
 * @param config Detector configuration (NULL for defaults)
 * @return Detector handle or NULL on failure
 */
oscillation_detector_t* oscillation_detector_create(const oscillation_detector_config_t* config);

/**
 * @brief Destroy oscillation detector and free resources
 */
void oscillation_detector_destroy(oscillation_detector_t* detector);

/**
 * @brief Add neural signal sample for analysis
 *
 * WHAT: Record continuous neural signal value
 * WHY:  Build time series for oscillation analysis
 * HOW:  Add to sliding window, apply online filtering
 *
 * @param detector Detector handle
 * @param signal Signal amplitude (population firing rate or LFP)
 * @param timestamp_ms Sample time in milliseconds
 * @return true on success, false on error
 */
bool oscillation_detector_add_sample(oscillation_detector_t* detector,
                                      float signal,
                                      double timestamp_ms);

/**
 * @brief Detect oscillations in current window
 *
 * WHAT: Analyze recent signal for oscillatory content
 * WHY:  Identify brain state and cognitive mode
 * HOW:  Compute PSD, band powers, detect bursts
 *
 * @param detector Detector handle
 * @param result Output oscillation analysis (required)
 * @return true on success, false on error
 */
bool oscillation_detector_detect(oscillation_detector_t* detector,
                                  oscillation_result_t* result);

/**
 * @brief Compute phase locking between two signals
 *
 * WHAT: Measure phase synchronization between channels
 * WHY:  Identify coordinated oscillations
 * HOW:  Hilbert transform + phase consistency analysis
 *
 * @param detector Detector handle
 * @param band Frequency band to analyze
 * @param signal1 First signal buffer
 * @param signal2 Second signal buffer
 * @param length Number of samples
 * @param result Output PLV analysis (required)
 * @return true on success, false on error
 */
bool oscillation_detector_compute_plv(oscillation_detector_t* detector,
                                       oscillation_band_t band,
                                       const float* signal1,
                                       const float* signal2,
                                       uint32_t length,
                                       phase_locking_t* result);

/**
 * @brief Detect cross-frequency coupling
 *
 * WHAT: Identify phase-amplitude coupling between bands
 * WHY:  Reveals information routing mechanisms
 * HOW:  Extract phase of low freq, amplitude of high freq, compute modulation
 *
 * @param detector Detector handle
 * @param couplings Output coupling results
 * @param max_couplings Maximum couplings to return
 * @param num_found Output: number of significant couplings
 * @return true on success, false on error
 */
bool oscillation_detector_detect_pac(oscillation_detector_t* detector,
                                      cross_freq_coupling_t* couplings,
                                      uint32_t max_couplings,
                                      uint32_t* num_found);

/**
 * @brief Get band power for specific frequency range
 *
 * @param detector Detector handle
 * @param band Frequency band
 * @param power Output: band power (required)
 * @return true on success, false on error
 */
bool oscillation_detector_get_band_power(const oscillation_detector_t* detector,
                                          oscillation_band_t band,
                                          band_power_t* power);

/**
 * @brief Reset detector state
 *
 * WHAT: Clear signal buffer and filter states
 * WHY:  Start fresh analysis
 * HOW:  Zero buffers, reset filters
 */
void oscillation_detector_reset(oscillation_detector_t* detector);

/**
 * @brief Get detector statistics
 *
 * @param detector Detector handle
 * @param total_samples Output: total samples processed
 * @param total_bursts Output: total bursts detected
 * @param avg_power Output: average total power
 * @return true on success, false on error
 */
bool oscillation_detector_get_stats(const oscillation_detector_t* detector,
                                     uint64_t* total_samples,
                                     uint64_t* total_bursts,
                                     float* avg_power);

/**
 * @brief Get default configuration
 */
oscillation_detector_config_t oscillation_detector_default_config(void);

/**
 * @brief Get band name as string
 */
const char* oscillation_band_name(oscillation_band_t band);

/**
 * @brief Get band frequency range
 */
void oscillation_band_range(oscillation_band_t band, float* min_hz, float* max_hz);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_OSCILLATION_DETECTOR_H
