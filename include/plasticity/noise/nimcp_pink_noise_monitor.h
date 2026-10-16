//=============================================================================
// nimcp_pink_noise_monitor.h - Real-Time Spectral Monitoring
//=============================================================================
/**
 * @file nimcp_pink_noise_monitor.h
 * @brief Real-time monitoring and auto-correction of pink noise spectrum
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Continuously monitor 1/f spectrum and auto-correct deviations
 * WHY:  Ensure generated noise maintains biological validity over time
 * HOW:  Sliding window FFT, online alpha estimation, feedback correction
 *
 * FEATURES:
 * =========
 * - Real-time spectral exponent (α) estimation
 * - Drift detection and correction
 * - Quality metrics and alerts
 * - Callback system for external monitoring
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PINK_NOISE_MONITOR_H
#define NIMCP_PINK_NOISE_MONITOR_H

#include <stdint.h>
#include <stdbool.h>
#include "plasticity/noise/nimcp_pink_noise.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define PINK_MONITOR_WINDOW_SIZE    512     /**< FFT window size */
#define PINK_MONITOR_HISTORY_SIZE   64      /**< History of α estimates */

//=============================================================================
// Callback Types
//=============================================================================

typedef void (*pink_monitor_alert_fn)(
    void* user_data,
    float measured_alpha,
    float target_alpha,
    const char* message
);

//=============================================================================
// Configuration
//=============================================================================

typedef struct {
    float target_alpha;             /**< Target spectral exponent */
    float tolerance;                /**< Acceptable deviation */
    float correction_gain;          /**< Feedback correction strength */
    uint32_t window_size;           /**< FFT window size */
    float update_rate;              /**< How often to update (samples) */
    bool enable_auto_correction;    /**< Enable automatic correction */
    bool enable_alerts;             /**< Enable alert callbacks */
    pink_monitor_alert_fn alert_callback;
    void* alert_user_data;
} pink_monitor_config_t;

//=============================================================================
// State Structure
//=============================================================================

typedef struct {
    pink_monitor_config_t config;
    pink_noise_generator_t generator;

    // Sample buffer for FFT
    float* sample_buffer;
    uint32_t buffer_index;
    bool buffer_full;

    // Spectral analysis
    float* power_spectrum;
    float* frequency_bins;
    float current_alpha;
    float alpha_history[PINK_MONITOR_HISTORY_SIZE];
    uint32_t history_index;

    // Correction state
    float amplitude_correction;
    float alpha_correction;
    bool correction_active;

    // Quality metrics
    float spectral_fit_r2;
    float alpha_variance;
    uint32_t drift_count;
    uint32_t correction_count;

    uint64_t total_samples;
    uint64_t last_update;
} pink_noise_monitor_t;

//=============================================================================
// Quality Metrics
//=============================================================================

typedef struct {
    float current_alpha;            /**< Current estimated α */
    float target_alpha;             /**< Target α */
    float alpha_deviation;          /**< |current - target| */
    float alpha_variance;           /**< Variance of α over history */
    float spectral_fit_r2;          /**< R² of power-law fit */
    bool in_tolerance;              /**< Within acceptable range */
    uint32_t drift_events;          /**< Number of drift detections */
    uint32_t corrections_applied;   /**< Number of corrections */
} pink_monitor_quality_t;

//=============================================================================
// API Functions
//=============================================================================

pink_monitor_config_t pink_monitor_default_config(void);
pink_noise_monitor_t* pink_monitor_create(const pink_monitor_config_t* config);
void pink_monitor_destroy(pink_noise_monitor_t* monitor);

/**
 * @brief Connect to pink noise generator
 */
int pink_monitor_connect(
    pink_noise_monitor_t* monitor,
    pink_noise_generator_t generator
);

/**
 * @brief Process new sample and update monitoring
 *
 * @param monitor Monitor instance
 * @param sample New noise sample
 * @return 0 on success, 1 if spectrum was updated, negative on error
 */
int pink_monitor_update(pink_noise_monitor_t* monitor, float sample);

/**
 * @brief Get current α estimate
 */
float pink_monitor_get_alpha(const pink_noise_monitor_t* monitor);

/**
 * @brief Get amplitude correction factor
 */
float pink_monitor_get_amplitude_correction(const pink_noise_monitor_t* monitor);

/**
 * @brief Get alpha correction factor
 */
float pink_monitor_get_alpha_correction(const pink_noise_monitor_t* monitor);

/**
 * @brief Get quality metrics
 */
int pink_monitor_get_quality(
    const pink_noise_monitor_t* monitor,
    pink_monitor_quality_t* quality
);

/**
 * @brief Force spectrum recalculation
 */
int pink_monitor_recalculate(pink_noise_monitor_t* monitor);

/**
 * @brief Reset monitor state
 */
int pink_monitor_reset(pink_noise_monitor_t* monitor);

/**
 * @brief Set alert callback
 */
int pink_monitor_set_callback(
    pink_noise_monitor_t* monitor,
    pink_monitor_alert_fn callback,
    void* user_data
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PINK_NOISE_MONITOR_H
