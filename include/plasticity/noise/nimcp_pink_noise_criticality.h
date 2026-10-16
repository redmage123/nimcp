//=============================================================================
// nimcp_pink_noise_criticality.h - Self-Organized Criticality Integration
//=============================================================================
/**
 * @file nimcp_pink_noise_criticality.h
 * @brief Pink noise as signature of neural criticality with avalanche dynamics
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Integrate pink noise with self-organized criticality and neural avalanches
 * WHY:  The brain operates near a critical point between order and chaos:
 *       - Pink noise (1/f spectrum) is signature of criticality
 *       - Neural avalanches follow power-law distributions
 *       - Critical dynamics optimize information transmission and storage
 *       - Deviation from criticality indicates pathological states
 *
 * HOW:  Monitor pink noise statistics to detect criticality state,
 *       generate avalanche-like bursts, and provide feedback for homeostasis.
 *
 * BIOLOGICAL BASIS:
 * =================
 * - Beggs & Plenz (2003): Neural avalanches in cortex
 * - Shew & Plenz (2013): Criticality and optimal computation
 * - Poil et al. (2012): 1/f noise as criticality signature
 * - Hesse & Gross (2014): Self-organized criticality in neural networks
 *
 * MATHEMATICAL MODEL:
 * ===================
 * Criticality Index (κ):
 *   κ = (α_measured - 1.0)² + (τ_measured - 1.5)²
 *   Where:
 *     α = spectral exponent (target: 1.0 for pink)
 *     τ = avalanche size exponent (target: ~1.5 for critical)
 *
 * Avalanche dynamics:
 *   P(S) ∝ S^(-τ)  for avalanche size S
 *   P(T) ∝ T^(-α)  for avalanche duration T
 *
 * Critical state: κ < threshold (near perfect criticality)
 * Subcritical: α > 1, τ > 1.5 (too ordered)
 * Supercritical: α < 1, τ < 1.5 (too chaotic)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PINK_NOISE_CRITICALITY_H
#define NIMCP_PINK_NOISE_CRITICALITY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "plasticity/noise/nimcp_pink_noise.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define CRITICALITY_HISTORY_SIZE    1024  /**< Samples for avalanche detection */
#define CRITICALITY_MAX_AVALANCHES  256   /**< Max avalanches to track */

//=============================================================================
// Criticality State
//=============================================================================

/**
 * @brief Criticality regime classification
 *
 * WHAT: Categories of dynamical state
 * WHY:  Different regimes have different computational properties
 */
typedef enum {
    CRITICALITY_SUBCRITICAL,    /**< Too ordered, low entropy */
    CRITICALITY_CRITICAL,       /**< Optimal: edge of chaos */
    CRITICALITY_SUPERCRITICAL,  /**< Too chaotic, high entropy */
    CRITICALITY_UNKNOWN         /**< Insufficient data */
} criticality_regime_t;

/**
 * @brief Single avalanche event
 */
typedef struct {
    uint64_t start_time;        /**< Start timestep */
    uint64_t duration;          /**< Duration in timesteps */
    float size;                 /**< Integrated activity */
    float peak_amplitude;       /**< Maximum amplitude */
} avalanche_event_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Criticality analysis configuration
 */
typedef struct {
    float threshold_high;       /**< Threshold for avalanche start (default: 2.0 std) */
    float threshold_low;        /**< Threshold for avalanche end (default: 0.5 std) */
    float target_alpha;         /**< Target spectral exponent (default: 1.0) */
    float target_tau;           /**< Target avalanche size exponent (default: 1.5) */
    float criticality_tolerance;/**< κ threshold for critical state (default: 0.1) */
    uint32_t min_avalanche_duration; /**< Minimum samples for valid avalanche */
    float sample_rate;          /**< Sampling rate (Hz) */
    bool enable_feedback;       /**< Enable homeostatic feedback */
    float feedback_gain;        /**< Gain for corrective feedback (default: 0.1) */
} criticality_config_t;

//=============================================================================
// State Structure
//=============================================================================

/**
 * @brief Criticality analyzer state
 */
typedef struct {
    criticality_config_t config;
    pink_noise_generator_t noise_generator;

    // Sample history for analysis
    float history[CRITICALITY_HISTORY_SIZE];
    uint32_t history_index;
    uint32_t history_count;

    // Statistics
    float running_mean;
    float running_std;
    float measured_alpha;
    float measured_tau;
    float criticality_index;
    criticality_regime_t regime;

    // Avalanche tracking
    avalanche_event_t avalanches[CRITICALITY_MAX_AVALANCHES];
    uint32_t num_avalanches;
    bool in_avalanche;
    avalanche_event_t current_avalanche;

    // Feedback state
    float amplitude_correction;
    float alpha_correction;

    uint64_t total_samples;
} criticality_analyzer_t;

/**
 * @brief Criticality statistics
 */
typedef struct {
    float spectral_exponent;        /**< Measured α */
    float avalanche_size_exponent;  /**< Measured τ */
    float criticality_index;        /**< Distance from criticality */
    criticality_regime_t regime;
    uint32_t num_avalanches;
    float avg_avalanche_size;
    float avg_avalanche_duration;
    float size_exponent_r2;         /**< R² for power-law fit */
    uint64_t total_samples;
} criticality_stats_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create criticality analyzer
 *
 * WHAT: Initialize analyzer for monitoring criticality state
 * WHY:  Track and maintain neural criticality
 * HOW:  Setup history buffers, connect to pink noise
 *
 * @param config Configuration
 * @return Analyzer handle or NULL on failure
 */
criticality_analyzer_t* criticality_create(const criticality_config_t* config);

/**
 * @brief Destroy criticality analyzer
 *
 * @param ca Analyzer to destroy (NULL-safe)
 */
void criticality_destroy(criticality_analyzer_t* ca);

/**
 * @brief Get default configuration
 *
 * @return Default criticality configuration
 */
criticality_config_t criticality_default_config(void);

//=============================================================================
// Analysis Functions
//=============================================================================

/**
 * @brief Process new sample and update analysis
 *
 * WHAT: Add sample to history, detect avalanches, update metrics
 * WHY:  Continuous monitoring of criticality state
 * HOW:  Update running stats, check thresholds, fit power laws
 *
 * @param ca Criticality analyzer
 * @param sample New sample value
 * @return 0 on success, negative on error
 */
int criticality_update(criticality_analyzer_t* ca, float sample);

/**
 * @brief Connect to pink noise generator
 *
 * WHAT: Link analyzer to noise source for feedback
 * WHY:  Enable homeostatic correction
 *
 * @param ca Criticality analyzer
 * @param generator Pink noise generator
 * @return 0 on success, negative on error
 */
int criticality_connect_generator(
    criticality_analyzer_t* ca,
    pink_noise_generator_t generator
);

/**
 * @brief Get current criticality regime
 *
 * @param ca Criticality analyzer
 * @return Current regime classification
 */
criticality_regime_t criticality_get_regime(const criticality_analyzer_t* ca);

/**
 * @brief Get criticality index (distance from critical point)
 *
 * WHAT: Measure how far from optimal criticality
 * WHY:  Quantify deviation for feedback control
 *
 * @param ca Criticality analyzer
 * @return Criticality index (0 = perfect, higher = worse)
 */
float criticality_get_index(const criticality_analyzer_t* ca);

/**
 * @brief Check if currently in avalanche
 *
 * @param ca Criticality analyzer
 * @return true if avalanche in progress
 */
bool criticality_in_avalanche(const criticality_analyzer_t* ca);

/**
 * @brief Get amplitude correction factor
 *
 * WHAT: Feedback signal for amplitude adjustment
 * WHY:  Homeostatic return to criticality
 *
 * @param ca Criticality analyzer
 * @return Multiplicative correction (1.0 = no change)
 */
float criticality_get_amplitude_correction(const criticality_analyzer_t* ca);

/**
 * @brief Get alpha correction factor
 *
 * WHAT: Feedback signal for spectral slope adjustment
 * WHY:  Tune noise spectrum to maintain criticality
 *
 * @param ca Criticality analyzer
 * @return Additive correction for alpha
 */
float criticality_get_alpha_correction(const criticality_analyzer_t* ca);

//=============================================================================
// Avalanche Functions
//=============================================================================

/**
 * @brief Generate avalanche-like burst
 *
 * WHAT: Create power-law distributed activity burst
 * WHY:  Simulate neural avalanche dynamics
 * HOW:  Generate size from P(S)∝S^(-τ), spread over duration
 *
 * @param ca Criticality analyzer
 * @param output Output buffer for avalanche
 * @param max_samples Maximum samples to generate
 * @param num_generated Output: actual samples generated
 * @return 0 on success, negative on error
 */
int criticality_generate_avalanche(
    criticality_analyzer_t* ca,
    float* output,
    uint32_t max_samples,
    uint32_t* num_generated
);

/**
 * @brief Get last N avalanches
 *
 * @param ca Criticality analyzer
 * @param avalanches Output array
 * @param max_count Maximum to return
 * @param count Output: actual count
 * @return 0 on success, negative on error
 */
int criticality_get_avalanches(
    const criticality_analyzer_t* ca,
    avalanche_event_t* avalanches,
    uint32_t max_count,
    uint32_t* count
);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get comprehensive statistics
 *
 * @param ca Criticality analyzer
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int criticality_get_stats(
    const criticality_analyzer_t* ca,
    criticality_stats_t* stats
);

/**
 * @brief Reset analyzer state
 *
 * @param ca Criticality analyzer
 * @return 0 on success, negative on error
 */
int criticality_reset(criticality_analyzer_t* ca);

/**
 * @brief Get regime name as string
 *
 * @param regime Criticality regime
 * @return Human-readable name
 */
const char* criticality_regime_name(criticality_regime_t regime);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PINK_NOISE_CRITICALITY_H
