//=============================================================================
// nimcp_pink_noise_correlated.h - Correlated Multi-Channel Pink Noise
//=============================================================================
/**
 * @file nimcp_pink_noise_correlated.h
 * @brief Multivariate pink noise with configurable inter-channel correlations
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Generate multiple correlated pink noise channels
 * WHY:  Neuromodulators and brain regions exhibit correlated fluctuations:
 *       - Dopamine/Norepinephrine share LC pathways (~0.6 correlation)
 *       - Serotonin/Dopamine have inverse relationship (~-0.3)
 *       - Regional correlations reflect functional connectivity
 *
 * HOW:  Generate independent white noise, apply correlation matrix (Cholesky),
 *       then apply 1/f^α filtering to each correlated channel.
 *
 * BIOLOGICAL BASIS:
 * =================
 * - Locus coeruleus projects both dopamine and norepinephrine
 * - Raphe nuclei modulate serotonin broadly but correlate with others
 * - Cortical regions show correlated noise (resting state networks)
 * - Correlation structure reflects anatomical connectivity
 *
 * MATHEMATICAL MODEL:
 * ===================
 * 1. Generate N independent white noise vectors: W₁, W₂, ..., Wₙ
 * 2. Apply Cholesky decomposition: L = chol(Σ) where Σ is correlation matrix
 * 3. Correlated white noise: X = L × W
 * 4. Apply 1/f^α filter to each channel: Y_i = pink_filter(X_i, α_i)
 *
 * Result: Y₁, Y₂, ..., Yₙ are pink noise channels with specified correlations.
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PINK_NOISE_CORRELATED_H
#define NIMCP_PINK_NOISE_CORRELATED_H

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

#define PINK_NOISE_MAX_CHANNELS     16   /**< Maximum number of correlated channels */
#define PINK_NOISE_NEUROMOD_CHANNELS 4   /**< Standard neuromodulator count */

//=============================================================================
// Channel Types
//=============================================================================

/**
 * @brief Predefined channel configurations
 *
 * WHAT: Common correlation patterns for biological systems
 * WHY:  Easy setup for standard use cases
 */
typedef enum {
    PINK_CORR_NEUROMODULATORS,   /**< DA, 5-HT, ACh, NE with biological correlations */
    PINK_CORR_BRAIN_REGIONS,     /**< For simulating regional correlations */
    PINK_CORR_INDEPENDENT,       /**< No correlation (identity matrix) */
    PINK_CORR_CUSTOM             /**< User-defined correlation matrix */
} pink_noise_correlation_type_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Per-channel configuration
 */
typedef struct {
    const char* name;           /**< Channel name (e.g., "dopamine") */
    float alpha;                /**< Spectral exponent for this channel */
    float amplitude;            /**< RMS amplitude */
} pink_noise_channel_config_t;

/**
 * @brief Full correlated noise configuration
 */
typedef struct {
    uint32_t num_channels;                                /**< Number of channels */
    pink_noise_channel_config_t channels[PINK_NOISE_MAX_CHANNELS];
    float correlation_matrix[PINK_NOISE_MAX_CHANNELS * PINK_NOISE_MAX_CHANNELS];
    pink_noise_correlation_type_t correlation_type;
    float sample_rate;                                    /**< Sampling rate (Hz) */
    uint32_t seed;                                        /**< Random seed */
} pink_noise_correlated_config_t;

//=============================================================================
// State Structure
//=============================================================================

/**
 * @brief Correlated multi-channel pink noise state
 */
typedef struct {
    pink_noise_correlated_config_t config;
    pink_noise_generator_t generators[PINK_NOISE_MAX_CHANNELS];
    float cholesky_matrix[PINK_NOISE_MAX_CHANNELS * PINK_NOISE_MAX_CHANNELS];
    float current_values[PINK_NOISE_MAX_CHANNELS];
    float white_buffer[PINK_NOISE_MAX_CHANNELS];
    uint64_t sample_count;
    bool cholesky_valid;
} pink_noise_correlated_t;

/**
 * @brief Statistics for correlated channels
 */
typedef struct {
    float measured_correlations[PINK_NOISE_MAX_CHANNELS * PINK_NOISE_MAX_CHANNELS];
    float channel_means[PINK_NOISE_MAX_CHANNELS];
    float channel_stds[PINK_NOISE_MAX_CHANNELS];
    uint64_t total_samples;
} pink_noise_correlated_stats_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create correlated multi-channel pink noise generator
 *
 * WHAT: Initialize system with specified correlation structure
 * WHY:  Enable biologically realistic correlated neuromodulator noise
 * HOW:  Compute Cholesky decomposition, create per-channel generators
 *
 * @param config Correlated noise configuration
 * @return Generator handle or NULL on failure
 */
pink_noise_correlated_t* pink_noise_correlated_create(
    const pink_noise_correlated_config_t* config
);

/**
 * @brief Destroy correlated noise generator
 *
 * @param cn Generator to destroy (NULL-safe)
 */
void pink_noise_correlated_destroy(pink_noise_correlated_t* cn);

/**
 * @brief Get default neuromodulator correlation configuration
 *
 * WHAT: Returns biologically-motivated neuromodulator correlations
 * WHY:  Easy setup for DA, 5-HT, ACh, NE with realistic correlations
 *
 * CORRELATION MATRIX:
 *        DA    5-HT   ACh    NE
 * DA    1.00  -0.30  0.20   0.60
 * 5-HT -0.30   1.00  0.15  -0.20
 * ACh   0.20   0.15  1.00   0.25
 * NE    0.60  -0.20  0.25   1.00
 *
 * @return Default neuromodulator configuration
 */
pink_noise_correlated_config_t pink_noise_correlated_neuromod_config(void);

/**
 * @brief Get independent channels configuration
 *
 * @param num_channels Number of independent channels
 * @return Configuration with identity correlation matrix
 */
pink_noise_correlated_config_t pink_noise_correlated_independent_config(
    uint32_t num_channels
);

//=============================================================================
// Generation Functions
//=============================================================================

/**
 * @brief Generate next sample for all channels
 *
 * WHAT: Advance all channels by one sample with correlation
 * WHY:  Streaming correlated noise generation
 * HOW:  Generate white noise, apply Cholesky, filter to pink
 *
 * @param cn Correlated noise generator
 * @return 0 on success, negative on error
 */
int pink_noise_correlated_step(pink_noise_correlated_t* cn);

/**
 * @brief Get current value for specific channel
 *
 * @param cn Correlated noise generator
 * @param channel_index Channel index
 * @return Current noise value for that channel
 */
float pink_noise_correlated_get_channel(
    const pink_noise_correlated_t* cn,
    uint32_t channel_index
);

/**
 * @brief Get current values for all channels
 *
 * @param cn Correlated noise generator
 * @param values Output array (must be >= num_channels)
 * @return 0 on success, negative on error
 */
int pink_noise_correlated_get_all(
    const pink_noise_correlated_t* cn,
    float* values
);

/**
 * @brief Get channel by name
 *
 * @param cn Correlated noise generator
 * @param name Channel name (e.g., "dopamine")
 * @return Current value or 0.0 if not found
 */
float pink_noise_correlated_get_named(
    const pink_noise_correlated_t* cn,
    const char* name
);

/**
 * @brief Generate batch of samples
 *
 * @param cn Correlated noise generator
 * @param outputs Array of output arrays (one per channel)
 * @param num_samples Number of samples to generate
 * @return 0 on success, negative on error
 */
int pink_noise_correlated_generate_batch(
    pink_noise_correlated_t* cn,
    float** outputs,
    uint32_t num_samples
);

//=============================================================================
// Correlation Control
//=============================================================================

/**
 * @brief Update correlation between two channels
 *
 * WHAT: Dynamically adjust correlation coefficient
 * WHY:  Correlation may change with brain state
 *
 * @param cn Correlated noise generator
 * @param channel_i First channel index
 * @param channel_j Second channel index
 * @param correlation New correlation value (-1 to 1)
 * @return 0 on success, negative on error
 *
 * @note Triggers Cholesky recomputation
 */
int pink_noise_correlated_set_correlation(
    pink_noise_correlated_t* cn,
    uint32_t channel_i,
    uint32_t channel_j,
    float correlation
);

/**
 * @brief Set full correlation matrix
 *
 * @param cn Correlated noise generator
 * @param matrix New correlation matrix (row-major, must be positive semi-definite)
 * @return 0 on success, negative on error
 */
int pink_noise_correlated_set_matrix(
    pink_noise_correlated_t* cn,
    const float* matrix
);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Compute measured statistics
 *
 * @param cn Correlated noise generator
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int pink_noise_correlated_get_stats(
    const pink_noise_correlated_t* cn,
    pink_noise_correlated_stats_t* stats
);

/**
 * @brief Reset all channels
 *
 * @param cn Correlated noise generator
 * @param new_seed New random seed (0 = use configured)
 * @return 0 on success, negative on error
 */
int pink_noise_correlated_reset(
    pink_noise_correlated_t* cn,
    uint32_t new_seed
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PINK_NOISE_CORRELATED_H
