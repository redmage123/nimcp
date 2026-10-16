//=============================================================================
// nimcp_pink_noise_multiscale.h - Multi-Scale Hierarchical Pink Noise
//=============================================================================
/**
 * @file nimcp_pink_noise_multiscale.h
 * @brief Multi-scale hierarchical 1/f noise for cortical temporal dynamics
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Generates coupled pink noise at multiple timescales simultaneously
 * WHY:  Different cortical layers operate at different temporal scales:
 *       - Layer 2/3: Fast dynamics (~10-100ms) for sensory processing
 *       - Layer 4: Medium dynamics (~100ms-1s) for integration
 *       - Layer 5/6: Slow dynamics (~1-10s) for context and prediction
 *       This matches the hierarchical predictive coding architecture.
 *
 * HOW:  Generates independent pink noise at each scale, then couples them
 *       via top-down (slow→fast) and bottom-up (fast→slow) influences.
 *
 * BIOLOGICAL BASIS:
 * =================
 * - Cortical hierarchy has characteristic timescales (Murray et al., 2014)
 * - Higher areas have longer intrinsic timescales (Honey et al., 2012)
 * - Pink noise at each level maintains local criticality
 * - Cross-scale coupling enables information flow across timescales
 *
 * MATHEMATICAL MODEL:
 * ===================
 * For scale i with timescale τᵢ:
 *   noise_i(t) = pink_noise(α_i, τ_i) +
 *                β_up × noise_{i-1}(t) +      // Bottom-up influence
 *                β_down × noise_{i+1}(t)      // Top-down influence
 *
 * Where β_up and β_down control coupling strength between scales.
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PINK_NOISE_MULTISCALE_H
#define NIMCP_PINK_NOISE_MULTISCALE_H

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

#define PINK_NOISE_MAX_SCALES       8    /**< Maximum number of temporal scales */
#define PINK_NOISE_DEFAULT_SCALES   4    /**< Default: fast, medium, slow, ultra-slow */

//=============================================================================
// Scale Configuration
//=============================================================================

/**
 * @brief Configuration for a single temporal scale
 *
 * WHAT: Parameters for one level in the temporal hierarchy
 * WHY:  Each cortical layer has distinct temporal characteristics
 */
typedef struct {
    float timescale_ms;         /**< Characteristic timescale in milliseconds */
    float alpha;                /**< Spectral exponent for this scale (0.8-1.2) */
    float amplitude;            /**< RMS amplitude at this scale */
    float coupling_up;          /**< Coupling strength from faster scale (0-1) */
    float coupling_down;        /**< Coupling strength from slower scale (0-1) */
} pink_noise_scale_config_t;

/**
 * @brief Configuration for multi-scale pink noise system
 *
 * WHAT: Full configuration for hierarchical noise generation
 * WHY:  Defines the complete temporal hierarchy
 */
typedef struct {
    uint32_t num_scales;                              /**< Number of temporal scales */
    pink_noise_scale_config_t scales[PINK_NOISE_MAX_SCALES]; /**< Per-scale config */
    float global_amplitude;                           /**< Global amplitude scaling */
    float sample_rate;                                /**< Sampling rate (Hz) */
    uint32_t seed;                                    /**< Random seed (0=time) */
    bool enable_coupling;                             /**< Enable inter-scale coupling */
} pink_noise_multiscale_config_t;

//=============================================================================
// State Structure
//=============================================================================

/**
 * @brief Multi-scale pink noise generator state
 *
 * WHAT: Maintains state for all temporal scales
 * WHY:  Efficient generation of coupled hierarchical noise
 */
typedef struct {
    pink_noise_multiscale_config_t config;
    pink_noise_generator_t generators[PINK_NOISE_MAX_SCALES];
    float current_values[PINK_NOISE_MAX_SCALES];      /**< Current noise at each scale */
    float coupled_values[PINK_NOISE_MAX_SCALES];      /**< After coupling applied */
    uint64_t sample_count;                            /**< Total samples generated */
    uint64_t update_counters[PINK_NOISE_MAX_SCALES];  /**< Per-scale update counts */
} pink_noise_multiscale_t;

/**
 * @brief Statistics for multi-scale noise
 */
typedef struct {
    float scale_amplitudes[PINK_NOISE_MAX_SCALES];    /**< Measured amplitude per scale */
    float scale_alphas[PINK_NOISE_MAX_SCALES];        /**< Measured alpha per scale */
    float cross_correlations[PINK_NOISE_MAX_SCALES];  /**< Correlation with adjacent scales */
    float total_variance;                              /**< Total variance across scales */
    uint64_t total_samples;                           /**< Total samples generated */
} pink_noise_multiscale_stats_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create multi-scale pink noise generator
 *
 * WHAT: Initialize hierarchical noise system with coupled scales
 * WHY:  Enable multi-timescale exploration matching cortical dynamics
 * HOW:  Create independent generators per scale, setup coupling
 *
 * @param config Multi-scale configuration
 * @return Generator handle or NULL on failure
 */
pink_noise_multiscale_t* pink_noise_multiscale_create(
    const pink_noise_multiscale_config_t* config
);

/**
 * @brief Destroy multi-scale generator
 *
 * WHAT: Free all resources
 * WHY:  Prevent memory leaks
 *
 * @param ms Generator to destroy (NULL-safe)
 */
void pink_noise_multiscale_destroy(pink_noise_multiscale_t* ms);

/**
 * @brief Get default multi-scale configuration
 *
 * WHAT: Returns biologically-motivated defaults
 * WHY:  Easy starting point matching cortical hierarchy
 *
 * DEFAULT SCALES:
 * - Scale 0: 50ms (fast, sensory)
 * - Scale 1: 200ms (medium, integration)
 * - Scale 2: 1000ms (slow, working memory)
 * - Scale 3: 5000ms (ultra-slow, context)
 *
 * @return Default configuration
 */
pink_noise_multiscale_config_t pink_noise_multiscale_default_config(void);

//=============================================================================
// Generation Functions
//=============================================================================

/**
 * @brief Generate next sample at all scales
 *
 * WHAT: Advance all scales by one sample, apply coupling
 * WHY:  Streaming generation for real-time use
 * HOW:  Generate raw noise, apply inter-scale coupling
 *
 * @param ms Multi-scale generator
 * @return 0 on success, negative on error
 */
int pink_noise_multiscale_step(pink_noise_multiscale_t* ms);

/**
 * @brief Get current noise value at specific scale
 *
 * WHAT: Return coupled noise value for one scale
 * WHY:  Different modules use different timescales
 *
 * @param ms Multi-scale generator
 * @param scale_index Scale index (0 = fastest)
 * @return Noise value at that scale
 */
float pink_noise_multiscale_get_scale(
    const pink_noise_multiscale_t* ms,
    uint32_t scale_index
);

/**
 * @brief Get combined noise across all scales
 *
 * WHAT: Weighted sum of all scales
 * WHY:  Single value incorporating all timescales
 * HOW:  Sum with configurable weights per scale
 *
 * @param ms Multi-scale generator
 * @param weights Per-scale weights (NULL = equal weighting)
 * @return Combined noise value
 */
float pink_noise_multiscale_get_combined(
    const pink_noise_multiscale_t* ms,
    const float* weights
);

/**
 * @brief Generate batch of samples at all scales
 *
 * WHAT: Fill arrays with noise from each scale
 * WHY:  Efficient batch generation
 *
 * @param ms Multi-scale generator
 * @param outputs Array of output arrays (one per scale)
 * @param num_samples Number of samples to generate
 * @return 0 on success, negative on error
 */
int pink_noise_multiscale_generate_batch(
    pink_noise_multiscale_t* ms,
    float** outputs,
    uint32_t num_samples
);

//=============================================================================
// Coupling Control
//=============================================================================

/**
 * @brief Set coupling strength between scales
 *
 * WHAT: Adjust inter-scale influence
 * WHY:  Dynamic modulation of hierarchical coupling
 *
 * @param ms Multi-scale generator
 * @param scale_index Scale to modify
 * @param coupling_up Bottom-up coupling (0-1)
 * @param coupling_down Top-down coupling (0-1)
 * @return 0 on success, negative on error
 */
int pink_noise_multiscale_set_coupling(
    pink_noise_multiscale_t* ms,
    uint32_t scale_index,
    float coupling_up,
    float coupling_down
);

/**
 * @brief Set amplitude at specific scale
 *
 * WHAT: Adjust noise amplitude for one scale
 * WHY:  Dynamic modulation based on task demands
 *
 * @param ms Multi-scale generator
 * @param scale_index Scale to modify
 * @param amplitude New amplitude
 * @return 0 on success, negative on error
 */
int pink_noise_multiscale_set_amplitude(
    pink_noise_multiscale_t* ms,
    uint32_t scale_index,
    float amplitude
);

//=============================================================================
// Statistics and Analysis
//=============================================================================

/**
 * @brief Compute statistics for multi-scale noise
 *
 * WHAT: Analyze noise properties at each scale
 * WHY:  Validate hierarchical noise quality
 *
 * @param ms Multi-scale generator
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int pink_noise_multiscale_get_stats(
    const pink_noise_multiscale_t* ms,
    pink_noise_multiscale_stats_t* stats
);

/**
 * @brief Reset all scales to initial state
 *
 * WHAT: Clear state and reseed generators
 * WHY:  Start fresh for new trial
 *
 * @param ms Multi-scale generator
 * @param new_seed New seed (0 = use configured)
 * @return 0 on success, negative on error
 */
int pink_noise_multiscale_reset(
    pink_noise_multiscale_t* ms,
    uint32_t new_seed
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PINK_NOISE_MULTISCALE_H
